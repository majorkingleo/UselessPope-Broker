#include "Bf1942Query.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <utility>

#include <CpputilsDebug.h>
#include <format.h>
#include <string_utils.h>
#include <stderr_exception.h>
#include <dbi.h>

#include "App.h"
#include "bindtypes.h"
#include "logging.h"

using namespace Tools;
using namespace std::chrono_literals;

namespace {

// Round-boundary detection thresholds.
constexpr int kRoundResetMargin = 60;   // roundTimeRemain jumped up => round reset
constexpr int kTicketsNearZero  = 5;    // previous tickets effectively 0
constexpr int kTicketsFreshMin  = 50;   // a fresh round's tickets sit well above this

// A finished round whose players collectively produced almost no score and
// almost no kills was not actually played: the server ran unattended (nobody
// logged in, or everybody was AFK). Mark such archives as idle games.
constexpr int kIdleMaxTotalScore = 20;
constexpr int kIdleMaxTotalKills = 20;

// A BF1942 client connection keeps its own scoreboard counters. If the client
// dies and rejoins the same round, the server reports a fresh 0/0/0 session even
// though the round is still running. Score may legitimately drop by 2 points
// (one suicide) between two polls, so only a larger regression is treated as a
// new session.
constexpr int kScoreRegressionTolerance = 2;

// BF1942_PLAYER intentionally keeps rows of players who left, so a crash/rejoin
// can carry the earlier score. A long-running round may therefore accumulate
// rows from players who joined, never did anything and left hours ago. When the
// round is archived, a stale row is only kept if it has some score/kills/deaths:
// that player actually took part. Stale zero rows are ghosts and must not be
// written to P_BF1942_PLAYER.
constexpr int kPlayerPresenceGraceSeconds = 60;

int to_int( const std::string & s )
{
	try { return std::stoi( s ); } catch( ... ) { return 0; }
}

std::string get_field( const std::map<std::string,std::string> & f, const std::string & key )
{
	auto it = f.find( key );
	return it == f.end() ? std::string() : it->second;
}

std::string trim_copy( const std::string & s )
{
	const auto b = s.find_first_not_of( " \t\r\n" );
	if( b == std::string::npos )
		return {};
	const auto e = s.find_last_not_of( " \t\r\n" );
	return s.substr( b, e - b + 1 );
}

// The response is a backslash-delimited stream: \key\value\key\value\...
// It may start with a single leading byte on some implementations, so find the
// first backslash rather than assuming index 0.
std::map<std::string,std::string> parse_fields( const std::string & response )
{
	std::map<std::string,std::string> fields;

	std::size_t i = response.find( '\\' );
	if( i == std::string::npos )
		return fields;
	++i;   // skip the opening backslash

	while( i < response.size() ) {
		std::size_t key_end = response.find( '\\', i );
		std::string key;
		if( key_end == std::string::npos ) {
			key = response.substr( i );
			i = response.size();
		} else {
			key = response.substr( i, key_end - i );
			i = key_end + 1;
		}

		if( i >= response.size() ) {
			if( !key.empty() )
				fields[key] = "";
			break;
		}

		std::size_t val_end = response.find( '\\', i );
		std::string val;
		if( val_end == std::string::npos ) {
			val = response.substr( i );
			i = response.size();
		} else {
			val = response.substr( i, val_end - i );
			i = val_end + 1;
		}
		fields[key] = val;
	}

	return fields;
}

std::optional<std::map<std::string,std::string>> query_fields( const std::string & host,
                                                              int port,
                                                              int timeout_s,
                                                              const std::string & request )
{
	addrinfo hints = {};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;

	const std::string port_str = std::to_string( port );
	addrinfo * result = nullptr;
	if( getaddrinfo( host.c_str(), port_str.c_str(), &hints, &result ) != 0 ) {
		return std::nullopt;
	}

	addrinfo * ai = result;
	while( ai != nullptr && ai->ai_family != AF_INET )
		ai = ai->ai_next;
	if( ai == nullptr ) {
		freeaddrinfo( result );
		return std::nullopt;
	}

	int sock = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
	if( sock < 0 ) {
		freeaddrinfo( result );
		return std::nullopt;
	}

	const int t = timeout_s > 0 ? timeout_s : 3;
	timeval tv = { t, 0 };
	setsockopt( sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof( tv ) );

	const int sent = static_cast<int>( sendto( sock, request.data(),
		static_cast<int>( request.size() ), 0,
		ai->ai_addr, static_cast<socklen_t>( ai->ai_addrlen ) ) );
	if( sent < 0 ) {
		close( sock );
		freeaddrinfo( result );
		return std::nullopt;
	}

	std::string response;
	char buf[4096];
	for( ;; ) {
		const int n = static_cast<int>( recvfrom( sock, buf, sizeof( buf ), 0, nullptr, nullptr ) );
		if( n <= 0 )
			break;   // timeout, closed, or error
		response.append( buf, static_cast<std::size_t>( n ) );
		if( response.find( "\\queryid\\" ) != std::string::npos ||
		    response.find( "\\final\\" ) != std::string::npos )
			break;
		if( response.size() > 64 * 1024 )
			break;
	}

	close( sock );
	freeaddrinfo( result );

	if( response.empty() )
		return std::nullopt;

	return parse_fields( response );
}

// Some server builds answer only server/rules for \status\ and expose the
// player list under \players\; others (the reference server) include the
// players in \status\ itself. Ask for both and fold the player fields in, so
// one code path works for either kind.
std::optional<std::map<std::string,std::string>> fetch_snapshot( const std::string & host,
                                                                 int port,
                                                                 int timeout_s )
{
	auto status = query_fields( host, port, timeout_s, "\\status\\" );
	if( !status )
		return std::nullopt;

	auto players = query_fields( host, port, timeout_s, "\\players\\" );
	if( players && players->count( "playername_0" ) ) {
		for( const auto & kv : *players )
			(*status)[kv.first] = kv.second;
	}

	return status;
}

} // namespace

Bf1942Query::Bf1942Query( int default_port, int interval_s, int timeout_s, const std::string & hosts )
: BasicThread( "Bf1942Query" ),
  m_default_port( default_port > 0 ? default_port : 22000 ),
  m_interval_s( interval_s > 0 ? interval_s : 15 ),
  m_timeout_s( timeout_s > 0 ? timeout_s : 3 )
{
	// BF1942QueryHosts is comma-separated "host[:port]".
	std::string::size_type start = 0;
	while( start <= hosts.size() ) {
		const std::string::size_type comma = hosts.find( ',', start );
		std::string entry = trim_copy( hosts.substr( start,
			comma == std::string::npos ? std::string::npos : comma - start ) );

		if( !entry.empty() ) {
			std::string h = entry;
			int p = m_default_port;

			const std::string::size_type colon = entry.rfind( ':' );
			if( colon != std::string::npos ) {
				h = trim_copy( entry.substr( 0, colon ) );
				const int parsed = to_int( trim_copy( entry.substr( colon + 1 ) ) );
				if( parsed > 0 )
					p = parsed;
			}

			if( !h.empty() )
				m_static_targets.push_back( { h, p } );
		}

		if( comma == std::string::npos )
			break;
		start = comma + 1;
	}
}

void Bf1942Query::run()
{
	while( !APP.quit_request ) {
		const auto cycle_end = std::chrono::steady_clock::now() + std::chrono::seconds( m_interval_s );

		// Static hosts first, then everything discovery has seen (deduped).
		std::vector<Target> targets = m_static_targets;

		try {
			for( const auto & t : targets_from_db() ) {
				bool duplicate = false;
				for( const auto & s : targets ) {
					if( s.host == t.host && s.port == t.port ) {
						duplicate = true;
						break;
					}
				}
				if( !duplicate )
					targets.push_back( t );
			}
		} catch( const std::exception & e ) {
			log_message( __FILE__, __LINE__, __PRETTY_FUNCTION__,
				Tools::format( "BF1942 query: cannot read targets: %s", e.what() ) );
		}

		for( const auto & t : targets ) {
			if( APP.quit_request )
				break;
			query_and_store( t );
		}

		// Sleep in small slices so quit stays responsive.
		while( !APP.quit_request && std::chrono::steady_clock::now() < cycle_end ) {
			std::this_thread::sleep_for( 200ms );
		}
	}
}

std::vector<Bf1942Query::Target> Bf1942Query::targets_from_db()
{
	std::vector<Target> out;

	// BF1942_SERVER (written by the discovery task) stores the server IP in
	// `ip_address`, not `server_ip` (that is the column name in STATUS/PLAYER).
	auto erg = APP.db->select( "SELECT `ip_address` FROM `BF1942_SERVER`" );
	if( !erg ) {
		return out;
	}

	for( const auto & row : erg.row_list.values ) {
		if( !row.empty() && !row[0].empty() ) {
			out.push_back( { row[0], m_default_port } );
		}
	}

	return out;
}

void Bf1942Query::query_and_store( const Target & target )
{
	try {
		auto fields = fetch_snapshot( target.host, target.port, m_timeout_s );
		if( !fields ) {
			log_message( __FILE__, __LINE__, __PRETTY_FUNCTION__,
				Tools::format( "BF1942 query: no response from %s:%d (query disabled or wrong port?)",
					target.host, target.port ) );
			return;
		}

		// Previous snapshot (persistent across restarts).
		BF1942_STATUS old;
		const bool has_old = StdSqlSelect( *APP.db,
			Tools::format( "select %%%s from %s where `server_ip` = '%s' ",
				old.get_table_name(), old.get_table_name(), escape( target.host ) ),
			DBInList<DBBindType>() >> old ) > 0;

		const std::string new_map = get_field( *fields, "mapname" );
		const int new_rtr = to_int( get_field( *fields, "roundTimeRemain" ) );

		bool round_ended = false;
		if( has_old && !old.mapname.data.empty() ) {
			const int t1 = to_int( get_field( *fields, "tickets1" ) );
			const int t2 = to_int( get_field( *fields, "tickets2" ) );

			round_ended =
				( !new_map.empty() && new_map != old.mapname.data ) ||
				( new_rtr > old.round_time_remain.data + kRoundResetMargin ) ||
				( ( old.tickets1.data + old.tickets2.data ) <= kTicketsNearZero &&
				  ( t1 + t2 ) > kTicketsFreshMin );
		}

		if( round_ended ) {
			finalize_round( target.host, old );
		}

		store_status( target.host, *fields, old, has_old, round_ended );
		store_players( target.host, *fields );

		APP.db->commit();

		log_message( __FILE__, __LINE__, __PRETTY_FUNCTION__,
			Tools::format( "BF1942 query: %s:%d -> %s | map=%s | %s/%s players",
				target.host, target.port,
				get_field( *fields, "hostname" ),
				get_field( *fields, "mapname" ),
				get_field( *fields, "numplayers" ),
				get_field( *fields, "maxplayers" ) ) );
	} catch( const std::exception & e ) {
		log_message( __FILE__, __LINE__, __PRETTY_FUNCTION__,
			Tools::format( "BF1942 query error for %s:%d: %s", target.host, target.port, e.what() ) );
	}
}

void Bf1942Query::finalize_round( const std::string & server_ip, const BF1942_STATUS & old )
{
	const int winner = ( old.tickets1.data > old.tickets2.data ) ? 1
	                 : ( old.tickets2.data > old.tickets1.data ) ? 2 : 0;

	std::string victory_type = "unknown";
	if( old.tickets1.data <= 0 || old.tickets2.data <= 0 ) {
		victory_type = "tickets";
	} else if( old.round_time_remain.data <= 0 ) {
		victory_type = "time";
	}

	const std::string now = now_string();

	// BF1942_PLAYER keeps rows of players who left so a reconnect can carry
	// their totals. A stale row with score/kills/deaths still belongs to the
	// ended round (the player took part and left), but a stale row with all
	// counters at zero is a ghost of someone who never played and must not be
	// archived. Recent rows are always kept, even with 0/0/0.
	const int presence_grace_s = std::max( 2 * m_interval_s, kPlayerPresenceGraceSeconds );
	const std::string presence_filter =
		old.last_seen.data.empty()
			? std::string()
			: Tools::format( " and ( `last_seen` >= date_sub('%s', interval %d second) "
				"or `score` <> 0 or `kills` <> 0 or `deaths` <> 0 )",
				escape( old.last_seen.data ), presence_grace_s );

	// Aggregate the ended round's roster before it is archived. A server that
	// ran with nobody logged in still ends rounds, but with (almost) no score or
	// kills. Those are recorded as idle games, not real results.
	DBTypeInt player_count {};
	DBTypeInt score_sum {};
	DBTypeInt kill_sum {};
	if( StdSqlSelect( *APP.db,
		Tools::format( "select count(*), coalesce(sum(`score`),0), coalesce(sum(`kills`),0) "
			"from `BF1942_PLAYER` where `server_ip` = '%s'%s",
			escape( server_ip ), presence_filter ),
		DBInList<DBType>() >> player_count >> score_sum >> kill_sum ) <= 0 )
	{
		throw STDERR_EXCEPTION( Tools::format( "cannot read BF1942_PLAYER totals: %s", APP.db->get_error() ) );
	}

	const bool idle = player_count.data == 0 ||
		( score_sum.data <= kIdleMaxTotalScore && kill_sum.data <= kIdleMaxTotalKills );

	// GameSpy reports the configured round length (`roundTime`) and the time
	// left (`roundTimeRemain`); their difference is the time actually played.
	// This is the server's own clock, so it does not include broker polling
	// lag and does not count time spent paused.
	const int played_seconds = old.round_time.data > 0
		? std::max( 0, old.round_time.data - old.round_time_remain.data )
		: 0;

	BF1942_GAME game {};
	game.server_ip = server_ip;
	game.mapname = old.mapname.data;
	game.gametype = old.gametype.data;
	game.started_at = old.round_started_at.data.empty() ? old.last_seen.data : old.round_started_at.data;
	game.ended_at = now;
	game.round_time = played_seconds;
	game.winning_team = winner;
	game.tickets1 = old.tickets1.data;
	game.tickets2 = old.tickets2.data;
	game.victory_type = victory_type;
	game.idle = idle ? 1 : 0;
	game.setHist( BASE::HIST_TYPE::HIST_AN, "broker" );
	game.setHist( BASE::HIST_TYPE::HIST_AE, "broker" );
	game.setHist( BASE::HIST_TYPE::HIST_LO, "broker" );

	if( !StdSqlInsert( *APP.db, game ) ) {
		throw STDERR_EXCEPTION( Tools::format( "cannot insert into BF1942_GAME: %s", APP.db->get_error() ) );
	}

	const int game_id = APP.db->get_insert_id();

	// Archive the ended round's participants (including players who left after
	// playing, but not stale 0/0/0 ghosts).
	BF1942_PLAYER live;
	DBInLimit limit {};
	while( StdSqlSelect( *APP.db,
		Tools::format( "select %%%s from %s where `server_ip` = '%s'%s order by idx",
			live.get_table_name(), live.get_table_name(), escape( server_ip ), presence_filter ),
		DBInList<DBBindType>() >> live, limit ) > 0 )
	{
		P_BF1942_PLAYER p {};
		p = live;
		p.game_id = game_id;
		p.mapname = old.mapname.data;
		p.winning_team = winner;
		p.setHist( BASE::HIST_TYPE::HIST_LO, "broker" );
		p.idx.data = 0;

		if( !StdSqlInsert( *APP.db, p ) ) {
			throw STDERR_EXCEPTION( Tools::format( "cannot insert into P_BF1942_PLAYER: %s", APP.db->get_error() ) );
		}
	}

	// The new round starts with an empty roster.
	APP.db->exec( Tools::format( "delete from %s where `server_ip` = '%s'",
		live.get_table_name(), escape( server_ip ) ) );

	log_message( __FILE__, __LINE__, __PRETTY_FUNCTION__,
		Tools::format( "BF1942 round archived: %s map=%s winner=team %d (%s) tickets %d:%d "
			"round_time=%d players=%d score=%d kills=%d idle=%d game_id=%d",
			server_ip, old.mapname.data, winner, victory_type,
			old.tickets1.data, old.tickets2.data, played_seconds,
			player_count.data, score_sum.data, kill_sum.data, idle ? 1 : 0, game_id ) );
}

void Bf1942Query::store_status( const std::string & server_ip,
                                const std::map<std::string,std::string> & fields,
                                const BF1942_STATUS & old,
                                bool has_old,
                                bool round_ended )
{
	const std::string now = now_string();

	BF1942_STATUS s = old;   // copies idx + hist + all live fields by name
	if( !has_old ) {
		s.server_ip = server_ip;
	}

	s.hostname = get_field( fields, "hostname" );
	s.mapname = get_field( fields, "mapname" );
	s.gametype = get_field( fields, "gametype" );
	s.hostport = to_int( get_field( fields, "hostport" ) );
	s.numplayers = to_int( get_field( fields, "numplayers" ) );
	s.maxplayers = to_int( get_field( fields, "maxplayers" ) );
	s.round_time = to_int( get_field( fields, "roundTime" ) );
	s.round_time_remain = to_int( get_field( fields, "roundTimeRemain" ) );
	s.tickets1 = to_int( get_field( fields, "tickets1" ) );
	s.tickets2 = to_int( get_field( fields, "tickets2" ) );
	s.status = to_int( get_field( fields, "status" ) );
	s.last_seen = now;

	if( !has_old || round_ended ) {
		s.round_started_at = now;   // a new round starts now
	}

	if( has_old ) {
		s.setHist( BASE::HIST_TYPE::HIST_AE, "broker" );
		if( !StdSqlUpdate( *APP.db, s,
				Tools::format( "where `%s` = '%d'", s.idx.get_name(), s.idx.data ) ) ) {
			throw STDERR_EXCEPTION( Tools::format( "cannot update BF1942_STATUS: %s", APP.db->get_error() ) );
		}
	} else {
		s.setHist( BASE::HIST_TYPE::HIST_AN, "broker" );
		s.setHist( BASE::HIST_TYPE::HIST_AE, "broker" );
		s.setHist( BASE::HIST_TYPE::HIST_LO, "broker" );
		if( !StdSqlInsert( *APP.db, s ) ) {
			throw STDERR_EXCEPTION( Tools::format( "cannot insert into BF1942_STATUS: %s", APP.db->get_error() ) );
		}
	}
}

void Bf1942Query::store_players( const std::string & server_ip,
                                 const std::map<std::string,std::string> & fields )
{
	const int maxplayers = to_int( get_field( fields, "maxplayers" ) );
	const int cap = maxplayers > 0 ? maxplayers : 128;
	const std::string now = now_string();

	// The server occasionally lists the same player twice (a ghost slot from a
	// reconnect). Since BF1942_PLAYER is keyed by player name, fold duplicates
	// and keep the entry with the most progress. Otherwise a zeroed duplicate
	// could look like a reconnect and destroy the real session's totals.
	struct RawPlayer {
		int slot;
		std::string name;
		std::string score;
		std::string kills;
		std::string deaths;
		std::string ping;
		std::string team;
	};

	std::vector<RawPlayer> players;
	std::map<std::string, std::size_t> player_by_name;

	auto progress = []( const RawPlayer & p ) {
		return to_int( p.score ) + 2 * to_int( p.kills ) + to_int( p.deaths );
	};

	for( int i = 0; i < cap; ++i ) {
		const std::string name = get_field( fields, "playername_" + std::to_string( i ) );
		if( name.empty() )
			break;

		RawPlayer player {
			i,
			name,
			get_field( fields, "score_" + std::to_string( i ) ),
			get_field( fields, "kills_" + std::to_string( i ) ),
			get_field( fields, "deaths_" + std::to_string( i ) ),
			get_field( fields, "ping_" + std::to_string( i ) ),
			get_field( fields, "team_" + std::to_string( i ) )
		};

		auto it = player_by_name.find( name );
		if( it == player_by_name.end() ) {
			player_by_name[name] = players.size();
			players.push_back( std::move( player ) );
			continue;
		}

		RawPlayer & existing_player = players[it->second];
		if( progress( player ) > progress( existing_player ) )
			existing_player = std::move( player );

		log_message( __FILE__, __LINE__, __PRETTY_FUNCTION__,
			Tools::format( "BF1942 query: %s lists player %s more than once; keeping slot %d",
				server_ip, name, players[it->second].slot ) );
	}

	for( const RawPlayer & player : players ) {
		const int i = player.slot;
		const std::string & name = player.name;
		const std::string & score = player.score;
		const std::string & kills = player.kills;
		const std::string & deaths = player.deaths;
		const std::string & ping = player.ping;
		const std::string & team = player.team;

		BF1942_PLAYER existing;
		const bool found = StdSqlSelect( *APP.db,
			Tools::format( "select %%%s from %s where `server_ip` = '%s' and `player_name` = '%s' ",
				existing.get_table_name(),
				existing.get_table_name(),
				escape( server_ip ),
				escape( name ) ),
			DBInList<DBBindType>() >> existing ) > 0;

		const int raw_score = to_int( score );
		const int raw_kills = to_int( kills );
		const int raw_deaths = to_int( deaths );

		if( found ) {
			// The stored score/kills/deaths are round totals; derive the current
			// client session's raw counters by removing the carried offsets.
			const int prev_raw_score = existing.score.data - existing.score_offset.data;
			const int prev_raw_kills = existing.kills.data - existing.kills_offset.data;
			const int prev_raw_deaths = existing.deaths.data - existing.deaths_offset.data;

			const bool counters_decreased =
				raw_kills < prev_raw_kills || raw_deaths < prev_raw_deaths;
			const bool back_to_zero =
				raw_score == 0 && raw_kills == 0 && raw_deaths == 0 &&
				( prev_raw_score != 0 || prev_raw_kills != 0 || prev_raw_deaths != 0 );
			const bool score_regressed =
				raw_score + kScoreRegressionTolerance < prev_raw_score &&
				raw_kills <= prev_raw_kills && raw_deaths <= prev_raw_deaths;

			if( counters_decreased || back_to_zero || score_regressed ) {
				// Same round, new client session: keep everything the player
				// earned before the reconnect and start adding from zero again.
				existing.score_offset = existing.score.data;
				existing.kills_offset = existing.kills.data;
				existing.deaths_offset = existing.deaths.data;

				log_message( __FILE__, __LINE__, __PRETTY_FUNCTION__,
					Tools::format( "BF1942 player %s on %s reconnected: carrying score=%d kills=%d deaths=%d",
						name, server_ip,
						existing.score_offset.data,
						existing.kills_offset.data,
						existing.deaths_offset.data ) );
			}

			existing.slot = i;
			existing.score = raw_score + existing.score_offset.data;
			existing.kills = raw_kills + existing.kills_offset.data;
			existing.deaths = raw_deaths + existing.deaths_offset.data;
			existing.ping = to_int( ping );
			existing.team = to_int( team );
			existing.last_seen = now;
			existing.setHist( BASE::HIST_TYPE::HIST_AE, "broker" );

			if( !StdSqlUpdate( *APP.db, existing,
					Tools::format( "where `%s` = '%d'", existing.idx.get_name(), existing.idx.data ) ) ) {
				throw STDERR_EXCEPTION( Tools::format( "cannot update BF1942_PLAYER: %s", APP.db->get_error() ) );
			}
		} else {
			BF1942_PLAYER p {};
			p.server_ip = server_ip;
			p.player_name = name;
			p.slot = i;
			p.score = raw_score;
			p.kills = raw_kills;
			p.deaths = raw_deaths;
			p.score_offset = 0;
			p.kills_offset = 0;
			p.deaths_offset = 0;
			p.ping = to_int( ping );
			p.team = to_int( team );
			p.last_seen = now;
			p.setHist( BASE::HIST_TYPE::HIST_AN, "broker" );
			p.setHist( BASE::HIST_TYPE::HIST_AE, "broker" );
			p.setHist( BASE::HIST_TYPE::HIST_LO, "broker" );

			if( !StdSqlInsert( *APP.db, p ) ) {
				throw STDERR_EXCEPTION( Tools::format( "cannot insert into BF1942_PLAYER: %s", APP.db->get_error() ) );
			}
		}
	}
}

void Bf1942Query::log_message( const char * file, unsigned line, const char * function,
                               const std::string & message )
{
	if( Tools::Debug * bf = bf_network_log() ) {
		bf->add( file, line, function, message );
	} else if( Tools::x_debug ) {
		// Dedicated log unavailable: fall back to the main logger.
		Tools::x_debug->add( file, line, function, message );
	}
}

bool Bf1942Query::query_once( const std::string & host, int port, int timeout_s )
{
	auto fields = fetch_snapshot( host, port, timeout_s );
	if( !fields ) {
		std::cerr << "error: no response from " << host << ':' << port
		          << " (query disabled or wrong port?)" << std::endl;
		return false;
	}

	const auto & f = *fields;
	const int numplayers = to_int( get_field( f, "numplayers" ) );
	const int maxplayers = to_int( get_field( f, "maxplayers" ) );

	std::cout << get_field( f, "hostname" )
	          << " | map=" << ( get_field( f, "mapname" ).empty() ? "?" : get_field( f, "mapname" ) )
	          << " | type=" << ( get_field( f, "gametype" ).empty() ? "?" : get_field( f, "gametype" ) )
	          << " | " << numplayers << "/" << maxplayers << " players\n";

	const int cap = maxplayers > 0 ? maxplayers : 128;
	for( int i = 0; i < cap; ++i ) {
		const std::string name = get_field( f, "playername_" + std::to_string( i ) );
		if( name.empty() )
			break;

		std::cout << name
		          << "  score=" << get_field( f, "score_" + std::to_string( i ) )
		          << " kills=" << get_field( f, "kills_" + std::to_string( i ) )
		          << " deaths=" << get_field( f, "deaths_" + std::to_string( i ) )
		          << " ping=" << get_field( f, "ping_" + std::to_string( i ) )
		          << " team=" << get_field( f, "team_" + std::to_string( i ) )
		          << "\n";
	}

	return true;
}

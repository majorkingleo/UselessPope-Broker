#include "ResolveUsernames.h"

#include "App.h"
#include "UsernameResolver.h"
#include "bindtypes.h"

#include <CpputilsDebug.h>
#include <dbi.h>
#include <format.h>
#include <stderr_exception.h>

#include <cstddef>

namespace {

const unsigned BATCH_SIZE = 100;

// Resolve users for all archived chunks that have no hist_an_user. The active
// queue is not touched here: it is always resolved before it is archived.
void resolve_archived_chunks()
{
	int last_idx = 0;
	std::size_t updated = 0;

	for( ;; ) {
		P_PLAY_QUEUE_CHUNKS chunks[BATCH_SIZE];
		DBInLimit limit( BATCH_SIZE );

		const int count = StdSqlSelect( *APP.db,
				Tools::format( "select %%P_PLAY_QUEUE_CHUNKS from P_PLAY_QUEUE_CHUNKS "
							   "where hist_an_user = '' and idx > %d order by idx",
							   last_idx ),
				DBInArrayList() >> chunks, limit );

		if( count < 0 ) {
			throw STDERR_EXCEPTION( Tools::format( "cannot select chunks without username: %s",
					APP.db->get_error() ) );
		}

		if( count == 0 ) {
			break;
		}

		for( int i = 0; i < count; ++i ) {
			P_PLAY_QUEUE_CHUNKS & chunk = chunks[i];
			last_idx = chunk.idx.data;

			const std::string username = UsernameResolver::from_file( chunk.file.data );

			if( username.empty() ) {
				continue;
			}

			chunk.hist_an_user.data = username;
			chunk.setHist( BASE::HIST_TYPE::HIST_AE, "broker" );

			if( !StdSqlUpdate( *APP.db, chunk,
					Tools::format( " where `%s` = %d",
							chunk.idx.get_name(), chunk.idx.data ) ) ) {
				CPPDEBUG( Tools::format( "cannot update chunk %d: %s",
						chunk.idx.data, APP.db->get_error() ) );
				continue;
			}

			++updated;
		}

		APP.db->commit();
	}

	CPPDEBUG( Tools::format( "resolved usernames in %d archived chunk(s)",
			static_cast<int>( updated ) ) );
}

// SERMON stores the actor as part of the action text ("<user> hat ...").
// Rewrite only entries whose action still starts with the empty username.
void rewrite_protocol_actions()
{
	const std::string sql =
			"UPDATE SERMON s "
			"JOIN P_PLAY_QUEUE_CHUNKS p ON p.sermon_reaction_idx = s.idx "
			"SET s.action = CONCAT( p.hist_an_user, s.action ), "
			"    s.hist_ae_user = 'broker', "
			"    s.hist_ae_zeit = NOW() "
			"WHERE s.action LIKE ' hat %' "
			"  AND p.hist_an_user <> ''";

	APP.db->exec( sql );
	APP.db->commit();
}

} // namespace

void resolve_protocol_usernames()
{
	resolve_archived_chunks();
	rewrite_protocol_actions();
}

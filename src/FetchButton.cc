#include "FetchButton.h"
#include "App.h"
#include <chrono>
#include <thread>
#include <stdexcept>
#include <chrono>
#include <CpputilsDebug.h>
#include <filesystem>
#include <list>
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <system_error>
#include <vector>
#include <sstream>
#include <format.h>
#include <string_utils.h>
#include <utf8_util.h>

using namespace std::chrono_literals;
using namespace std::chrono;

const std::string FetchButton::ACTION_BUTTON_PRESSED 	= "ButtonPressed";
const std::string FetchButton::ACTION_BUTTON_RELEASED 	= "ButtonReleased";
const std::string FetchButton::ACTION_PING 				= "Ping";

namespace {

// Maximum time between two ButtonReleased events that still belong to the same
// multi-click burst (measured with the remote button's millisecond clock).
unsigned constexpr CLICK_GAP_MS = 600;

// How long to wait after the last click before the burst is considered finished.
// Slightly longer than CLICK_GAP to compensate for the 100 ms poll interval.
auto constexpr CLICK_GAP_TIMEOUT = std::chrono::milliseconds( 800 );

// Deduplication window for the duplicated UDP datagrams of the remote button.
auto constexpr SEEN_EVENT_TTL = std::chrono::seconds( 10 );
std::size_t constexpr MAX_SEEN_EVENTS_PER_KEY = 32;

// Lexically case-insensitive ordering on wide strings. Names that only differ
// in case fall back to a case-sensitive comparison so the order is deterministic.
bool wide_case_insensitive_less( const std::wstring & a, const std::wstring & b )
{
	const std::wstring la = Tools::tolower( a );
	const std::wstring lb = Tools::tolower( b );

	if( la != lb ) {
		return la < lb;
	}

	return a < b;
}

std::string click_burst_key( const BUTTON_QUEUE & bq, const USERS_ACTION & user )
{
	if( !bq.mac_address.data.empty() ) {
		return bq.mac_address.data;
	}

	return "user:" + user.username.data;
}

} // namespace

FetchButton::FetchButton()
: BasicThread( "FetchButton" )
{

}

void FetchButton::run()
{
	auto user_refresh_deadline = steady_clock::now() + 30s;

    while( !APP.quit_request ) {

    	if( ( m_users_actions_by_mac_address.empty()
	 		  && m_users_actions_by_username.empty() 
			)
		 	|| steady_clock::now() >  user_refresh_deadline ) {
    		fetch_users();
    	}

        fetch_buttons();

        APP.db->commit();

        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
    }
}

void FetchButton::fetch_buttons()
{
	BUTTON_QUEUE button_queue[100];
	DBInLimit limit(100);
	int count = 0;

	if( ( count = StdSqlSelect( *APP.db,
			Tools::format( "select %%%s from %s order by hist_an_zeit, idx",
				button_queue[0].get_table_name(),
				button_queue[0].get_table_name() ),
				DBInArrayList() >> button_queue, limit ) ) < 0 ) {
		throw std::runtime_error( Tools::format( "SqlError: %s", APP.db->get_error()) );
	}

	std::list<BUTTON_QUEUE> to_delete;

    for( int i = 0; i < count; i++ ) {
		const std::string & mac_address = button_queue[i].mac_address.data;
		const std::string & username = button_queue[i].hist_an_user.data;		

		BUTTON_QUEUE *pbq = nullptr;
		USERS_ACTION *pua = nullptr;

		if( !mac_address.empty() ) {
			auto it = m_users_actions_by_mac_address.find( mac_address );

			if( it == m_users_actions_by_mac_address.end() ) {
				CPPDEBUG( Tools::format( "couldn't find USERS_ACTIONS for mac: '%s' action: '%s'", 
					mac_address, button_queue[i].action.data ) );
				to_delete.emplace_back( std::move( button_queue[i] ) );
				continue;
			}
			
			pbq = &button_queue[i];
			pua = &it->second;

		} else {
			auto it = m_users_actions_by_username.find( username );

			if( it == m_users_actions_by_username.end() ) {				
				CPPDEBUG( Tools::format( "couldn't find USERS_ACTIONS for user: '%s'", username) );
				to_delete.emplace_back( std::move( button_queue[i] ) );
				continue;			
			}

			pbq = &button_queue[i];
			pua = &it->second;
		}

		if( !pbq || !pua ) {
			continue;
		}

		// Attribute physical button events to the resolved user before they are
		// archived to P_BUTTON_QUEUE. The raw UDP datagram only carries the MAC
		// address, so without this the archived row has an empty hist_an_user and
		// is invisible to the top-user statistics. Do not use setHist() here: it
		// would overwrite the original receive timestamp.
		if( pbq->hist_an_user.data.empty() ) {
			pbq->hist_an_user.data = pua->username.data;
		}

		// The remote button sends every datagram twice (broadcast + unicast).
		// Both copies are identical, so (MAC, SEQ, TIME) identifies one physical
		// event. Drop the second copy instead of counting it as another click.
		if( ( pbq->action.data == ACTION_BUTTON_PRESSED || pbq->action.data == ACTION_BUTTON_RELEASED )
			&& is_duplicate_button_event( *pbq ) ) {
			to_delete.emplace_back( std::move( *pbq ) );
			continue;
		}

		if( pbq->action.data == ACTION_PING ) {
			to_delete.emplace_back( std::move( *pbq ) );
			continue;
		}

		if( !pbq->file.data.empty() ) {

			PLAY_QUEUE_CHUNKS chunk{};

			CPPDEBUG( Tools::format( "searching for file: '%s' for user '%s'", 
					  pbq->file.data,
					  pua->username.data));

			std::string audio_random_file  = pua->home_directory.data + "/audio_random/" + pbq->file.data;
			std::string audio_main_file  = pua->home_directory.data + "/audio_main/" + pbq->file.data;

			std::string file;

			if( std::filesystem::exists( pbq->file.data ) ) {
				file = pbq->file.data;
			}
			else if( std::filesystem::exists( audio_random_file ) ) {
				file = audio_random_file;
			} else if( std::filesystem::exists( audio_main_file ) ) {
				file = audio_main_file;
			}

			if( file.empty() ) {
				to_delete.emplace_back( std::move( *pbq ) );
				continue;
			}

			chunk.file.data = file;
			chunk.setHist( BASE::HIST_TYPE::HIST_AN, pua->username.data );
			chunk.setHist( BASE::HIST_TYPE::HIST_AE, pua->username.data );
			chunk.setHist( BASE::HIST_TYPE::HIST_LO, pua->username.data );

			if( !StdSqlInsert( *APP.db, chunk ) ) {
				CPPDEBUG( Tools::format( "SqlError: %s", APP.db->get_error()));
				continue;
			}

			P_BUTTON_QUEUE p_button_queue{};
			p_button_queue = *pbq;
			p_button_queue.setHist( BASE::HIST_TYPE::HIST_LO, pua->username.data );
			p_button_queue.idx.data = 0;

			if( !StdSqlInsert( *APP.db, p_button_queue ) ) {
				CPPDEBUG( Tools::format( "SqlError: %s", APP.db->get_error()));
				continue;
			}

			APP.db->exec( Tools::format( "delete from %s where idx = %d", pbq->get_table_name(), pbq->idx() ) );
    		APP.db->commit();
		} else if( pbq->action.data == ACTION_BUTTON_PRESSED ) {
			// A click is evaluated when the button is released again; the press
			// itself only needs to be archived.
			to_delete.emplace_back( std::move( *pbq ) );
		} else {
			// Empty file: this is a physical remote-button click (ButtonReleased,
			// or a legacy request). Accumulate clicks and defer playback until the
			// burst is over, so double/triple/... clicks can be distinguished.
			handle_button_click( *pbq, *pua );
			to_delete.emplace_back( std::move( *pbq ) );
		}
    }

	// Play all bursts that did not receive another click within the click gap.
	flush_expired_click_bursts();

	for( auto & to_del : to_delete ) {
		delete_button_queue_entry( to_del );
	}

	APP.db->commit();
}

bool FetchButton::delete_button_queue_entry( BUTTON_QUEUE & bq )
{
	P_BUTTON_QUEUE p_button_queue = bq;
	p_button_queue.setHist( BASE::HIST_TYPE::HIST_LO, "broker" );
	p_button_queue.idx.data = 0;

	if( p_button_queue.action.data != ACTION_PING ) {
		if( !StdSqlInsert( *APP.db, p_button_queue ) ) {
			CPPDEBUG( Tools::format( "SqlError: %s", APP.db->get_error()));
			return false;
		}
	}

	APP.db->exec( Tools::format( "delete from %s where idx = %d", bq.get_table_name(), bq.idx() ) );
	APP.db->commit();

	return true;
}

bool FetchButton::is_duplicate_button_event( const BUTTON_QUEUE & bq )
{
	const std::string & mac = bq.mac_address.data;
	const unsigned seq = static_cast<unsigned>( bq.seq.data );
	const unsigned time_stamp = static_cast<unsigned>( bq.time_stamp.data );

	auto & seen = m_seen_button_events[mac];
	const auto now = steady_clock::now();

	while( !seen.empty() && ( now - seen.front().seen_at ) > SEEN_EVENT_TTL ) {
		seen.pop_front();
	}

	for( const auto & event : seen ) {
		if( event.seq == seq && event.time_stamp == time_stamp ) {
			CPPDEBUG( Tools::format( "duplicate button event mac='%s' seq=%u time=%u",
				mac, seq, time_stamp ) );
			return true;
		}
	}

	seen.push_back( SeenButtonEvent{ seq, time_stamp, now } );

	while( seen.size() > MAX_SEEN_EVENTS_PER_KEY ) {
		seen.pop_front();
	}

	return false;
}

void FetchButton::handle_button_click( const BUTTON_QUEUE & bq, const USERS_ACTION & user )
{
	const std::string key = click_burst_key( bq, user );
	const unsigned time_stamp = static_cast<unsigned>( bq.time_stamp.data );
	const auto now = steady_clock::now();

	auto it = m_click_bursts.find( key );

	if( it != m_click_bursts.end() ) {
		ClickBurst & burst = it->second;

		// Arduino millis() may wrap around; treat a wrap as a new burst.
		const bool same_burst = time_stamp >= burst.last_time_stamp
			&& ( time_stamp - burst.last_time_stamp ) <= CLICK_GAP_MS;

		if( same_burst ) {
			burst.count++;
			burst.last_time_stamp = time_stamp;
			burst.last_event = now;
			burst.user = user;

			CPPDEBUG( Tools::format( "button '%s': %u click(s) in burst",
				key, burst.count ) );
			return;
		}

		// A new burst started, so the previous one is complete.
		flush_click_burst( burst );
		m_click_bursts.erase( it );
	}

	ClickBurst burst {};
	burst.user = user;
	burst.count = 1;
	burst.last_time_stamp = time_stamp;
	burst.last_event = now;

	m_click_bursts[key] = burst;

	CPPDEBUG( Tools::format( "button '%s': new click burst", key ) );
}

void FetchButton::flush_click_burst( const ClickBurst & burst )
{
	if( burst.count == 0 ) {
		return;
	}

	const std::string & username = burst.user.username.data;
	const std::vector<std::string> files = get_audio_main_files( burst.user.home_directory.data );

	if( files.empty() ) {
		CPPDEBUG( Tools::format( "no files in '%s/audio_main', nothing to play",
			burst.user.home_directory.data ) );
		return;
	}

	// N clicks -> N-th file. Clamp to the supported range and to the number of
	// files that actually exist, so a 4th/5th click never reads out of bounds.
	unsigned file_index = std::min( burst.count, MAX_CLICK_COUNT );
	file_index = std::min( file_index, static_cast<unsigned>( files.size() ) );
	file_index -= 1;

	const std::string & file = files[file_index];

	CPPDEBUG( Tools::format( "%u click(s) from '%s' -> playing '%s'",
		burst.count, username, file ) );

	PLAY_QUEUE_CHUNKS chunk {};
	chunk.file.data = file;
	chunk.setHist( BASE::HIST_TYPE::HIST_AN, username );
	chunk.setHist( BASE::HIST_TYPE::HIST_AE, username );
	chunk.setHist( BASE::HIST_TYPE::HIST_LO, username );

	if( !StdSqlInsert( *APP.db, chunk ) ) {
		CPPDEBUG( Tools::format( "SqlError: %s", APP.db->get_error() ) );
		return;
	}

	APP.db->commit();
}

void FetchButton::flush_expired_click_bursts()
{
	const auto now = steady_clock::now();

	for( auto it = m_click_bursts.begin(); it != m_click_bursts.end(); ) {
		if( ( now - it->second.last_event ) >= CLICK_GAP_TIMEOUT ) {
			flush_click_burst( it->second );
			it = m_click_bursts.erase( it );
		} else {
			++it;
		}
	}
}

std::vector<std::string> FetchButton::get_audio_main_files( const std::string & home_directory ) const
{
	std::vector<std::wstring> wide_files;

	const std::string directory = home_directory + "/audio_main/";

	std::error_code ec;
	std::filesystem::directory_iterator dir( directory, ec );

	if( ec ) {
		CPPDEBUG( Tools::format( "cannot read directory '%s': %s",
			directory, ec.message() ) );
		return {};
	}

	for( const auto & entry : dir ) {
		std::error_code entry_ec;
		const bool is_file = entry.is_regular_file( entry_ec );

		if( entry_ec || !is_file ) {
			continue;
		}

		// Convert to wchar *before* sorting so multi-byte file names are not
		// split and are displayed correctly in the debug output.
		wide_files.push_back( Tools::Utf8Util::utf8toWString( entry.path().string() ) );
	}

	// Lexical, case-insensitive order: click N plays the N-th entry.
	std::sort( wide_files.begin(), wide_files.end(), wide_case_insensitive_less );

	// Log the sorted list (1-based, so it maps directly to the click count).
	std::wstringstream listing;
	for( std::size_t i = 0; i < wide_files.size(); ++i ) {
		listing << L"  [" << ( i + 1 ) << L"] " << wide_files[i] << L'\n';
	}

	CPPDEBUG( Tools::wformat( L"audio_main files for '%s' (sorted):\n%s",
		Tools::Utf8Util::utf8toWString( home_directory ),
		listing.str() ) );

	std::vector<std::string> files;
	files.reserve( wide_files.size() );

	for( const auto & file : wide_files ) {
		files.push_back( Tools::Utf8Util::wStringToUtf8( file ) );
	}

	return files;
}

void FetchButton::fetch_users()
{
	USERS_ACTION users_actions[20];
	DBInLimit limit(20);
	int count = 0;

	if( ( count = StdSqlSelect( *APP.db,
			Tools::format( "select %%%s from %s",
				users_actions[0].get_table_name(),
				users_actions[0].get_table_name() ),
				DBInArrayList() >> users_actions, limit ) ) < 0 ) {
		throw std::runtime_error( Tools::format( "SqlError: %s", APP.db->get_error()) );
	}

	m_users_actions_by_mac_address.clear();

    for( int i = 0; i < count; i++ ) {

		m_users_actions_by_username[users_actions[i].username.data] = users_actions[i];

    	if( users_actions[i].button_mac_address.data.empty() ) {
    		continue;
    	}

    	m_users_actions_by_mac_address[users_actions[i].button_mac_address.data] = users_actions[i];		
    }
}

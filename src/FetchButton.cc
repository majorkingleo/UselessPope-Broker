#include "FetchButton.h"
#include "App.h"
#include <chrono>
#include <thread>
#include <stdexcept>
#include <chrono>
#include <CpputilsDebug.h>
#include <filesystem>
#include <list>

using namespace std::chrono_literals;
using namespace std::chrono;

const std::string FetchButton::ACTION_BUTTON_PRESSED 	= "ButtonPressed";
const std::string FetchButton::ACTION_BUTTON_RELEASED 	= "ButtonReleased";
const std::string FetchButton::ACTION_PING 				= "Ping";

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
			Tools::format( "select %%%s from %s order by hist_an_zeit",
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
				continue;			
			}

			pbq = &button_queue[i];
			pua = &it->second;
		}

		if( !pbq || !pua ) {
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
		} else {

			PLAY_QUEUE_CHUNKS chunk{};

			CPPDEBUG( Tools::format( "searching for for users main file '%s'",
					  pua->username.data));

			std::string audio_main_file_path = pua->home_directory.data + "/audio_main/";

			bool found = false;

			for (const auto & entry : std::filesystem::directory_iterator(audio_main_file_path)) {
   				 CPPDEBUG( Tools::format( "found: '%s'",  entry.path().string() ) );

				chunk.file.data = entry.path().string();
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
				break;
  			}

			if( !found ) {
				to_delete.emplace_back( std::move( *pbq ) );
			}

		}
    }

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

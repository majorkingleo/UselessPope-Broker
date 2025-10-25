#include "FetchButton.h"
#include "App.h"
#include <chrono>
#include <thread>
#include <stdexcept>
#include <chrono>
#include <CpputilsDebug.h>

using namespace std::chrono_literals;
using namespace std::chrono;

FetchButton::FetchButton()
: BasicThread( "FetchButton" )
{

}

void FetchButton::run()
{
	auto user_refresh_deadline = steady_clock::now() + 30s;

    while( !APP.quit_request ) {

    	if( m_users_actions_by_mac_address.empty() || steady_clock::now() >  user_refresh_deadline ) {
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

    for( int i = 0; i < count; i++ ) {
		std::string mac_address = button_queue[i].mac_address.data;
		auto it = m_users_actions_by_mac_address.find( mac_address );

		if( it == m_users_actions_by_mac_address.end() ) {
			CPPDEBUG( Tools::format( "couldn't find USERS_ACTIONS for max: '%s'", mac_address) )
		}
    }
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
    	if( users_actions[i].button_mac_address.data.empty() ) {
    		continue;
    	}

    	m_users_actions_by_mac_address[users_actions[i].button_mac_address.data] = users_actions[i];
    }
}

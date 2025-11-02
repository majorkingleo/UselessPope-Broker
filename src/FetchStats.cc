#include "FetchStats.h"
#include <chrono>
#include "App.h"
#include <thread>
#include <CpputilsDebug.h>
#include <format.h>

using namespace std::chrono_literals;
using namespace std::chrono;
using namespace Tools;

FetchStats::FetchStats()
: BasicThread( "FetchStats" )
{

}

void FetchStats::run()
{
	const auto timeout = 1min;
	auto deadline = steady_clock::now();

    while( !APP.quit_request ) {

    	if( steady_clock::now() > deadline ) {
    		APP.db->commit();

    		try {
    			fetch_total_actions();

    		} catch( const std::exception & error ) {
    			CPPDEBUG( Tools::format( "Error: %s", error.what() ));
    		}

    		APP.db->commit();
    		deadline = steady_clock::now() + timeout;
    	}

        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
    }
}

void FetchStats::fetch_total_actions()
{
	unsigned total_count {};

	total_count += count_table( P_PLAY_QUEUE_CHUNKS{} );
	total_count += count_table( P_PLAY_QUEUE_MUSIC{} );
	total_count += count_table( P_PLAY_QUEUE_ANIMATION{} );
	total_count += count_table( P_BUTTON_QUEUE{} );

	STATS stats = fetch_stats( TOTAL_ACTIONS );
	stats.value.data = Tools::format( "%d", total_count );
	stats.setHist( BASE::HIST_TYPE::HIST_AE, "broker" );

	std::string where = Tools::format( " where idx='%d'", stats.idx.data );

	StdSqlUpdate( *APP.db, stats, where );

	APP.db->commit();
}

unsigned FetchStats::count_table( const BASE & table )
{
	std::string sql = Tools::format( "select count(idx) from %s", table.get_table_name() );

	DBTypeInt count;

	if( StdSqlSelect( *APP.db, sql, DBInList() >> count ) < 0 ) {
		CPPDEBUG( Tools::format( "Sql: %s, Error: %s", sql, APP.db->get_error() ) );
		throw std::runtime_error( Tools::format( "SqlError: %s", APP.db->get_error() ) );
	}

	CPPDEBUG( Tools::format( "count(%s): %d",  table.get_table_name(), count.data ) );

	return count.data;
}

STATS FetchStats::fetch_stats( const std::string & key )
{
	STATS stats {};

	if( StdSqlSelect( *APP.db,
			Tools::format( "select %%%s from %s where `%s` = '%s' ",
					stats.get_table_name(),
					stats.get_table_name(),
					stats.key.get_name(),
					escape( key ) ),
			DBInList<DBBindType>() >> stats ) <= 0 ) {

		stats.setHist( BASE::HIST_TYPE::HIST_AN, "broker" );
		stats.setHist( BASE::HIST_TYPE::HIST_AE, "broker" );
		stats.setHist( BASE::HIST_TYPE::HIST_LO, "broker" );

		return stats;
	}

	return stats;
}

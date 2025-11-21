#include "AutoStopFog.h"
#include "App.h"
#include "bindtypes.h"
#include <CpputilsDebug.h>
#include <chrono>
#include <stderr_exception.h>

using namespace std::chrono_literals;

AutoStopFog::AutoStopFog()
: BasicThread( "AutoStopFog" )
{

}

void AutoStopFog::run()
{
    std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + 10s;

    while( !APP.quit_request ) {
    
       if( !APP.db ) {
            APP.reconnect_db();
        }

        if( std::chrono::steady_clock::now() > deadline ) {
            
            try {                
                auto_deactivate();
            } catch( const std::exception & error ) {
                CPPDEBUG( Tools::format( "error: %s", error.what() ) );                
            }

            deadline =  std::chrono::steady_clock::now() + 10s;
        }
 
        APP.db->commit();
        std::this_thread::sleep_for( 1s );
    }
}

void AutoStopFog::auto_deactivate()
{
    CONFIG config {};

    int count = StdSqlSelect( *APP.db,  Tools::format( "select %%%s from `%s` where `%s` = 'fog' and `%s` = '1' ",
        config.get_table_name(),
        config.get_table_name(),            
        config.key.get_name(),
        config.value.get_name() ),
        DBInList<DBBindType>() >> config );

    CPPDEBUG( Tools::format( "count: %d %s", count, APP.db->get_sql()  ));

    if( count > 0 ) {
        auto res = config.hist_ae_zeit.get_time();

        if( !res ) {
            throw STDERR_EXCEPTION( res.error() );
        }

        time_t last_change = res.value();
        time_t now = time(NULL);

        if( last_change + 60 < now ) {
            APP.db->exec( Tools::format( "update `%s` set `%s` = '0' where `%s` != '0' "
                            " and `%s` ",
                            config.get_table_name(),
                            config.value.get_name(),
                            config.key.get_name() ) );
            APP.db->commit();
        }
    }
}
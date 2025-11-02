#include "FetchStats.h"
#include <chrono>
#include "App.h"
#include <thread>

using namespace std::chrono_literals;

FetchStats::FetchStats()
: BasicThread( "FetchStats" )
{

}

void FetchStats::run()
{


    while( !APP.quit_request ) {

        APP.db->commit();
        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
    }
}


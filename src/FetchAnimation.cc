#include "FetchAnimation.h"
#include "App.h"
#include "bindtypes.h"
#include <CpputilsDebug.h>

using namespace Tools;
using namespace std::chrono_literals;

FetchAnimation::FetchAnimation( PlayAnimation & pa )
: BasicThread( "FetchAnimation" ),
 play_animation( pa )
 {

 }

void FetchAnimation::run()
{
    while( !APP.quit_request ) {
    
        fetch_animations();
        play_animation.run_once();

        if( !APP.db ) {
            APP.reconnect_db();
        }

        APP.db->commit();
        std::this_thread::sleep_for( 100ms );
    }
}

void FetchAnimation::fetch_animations()
{
    if( play_animation.countAnimationsInQueue() > 1 ) {
        CPPDEBUG( Tools::format( "countAnimationsInQueue: %d", play_animation.countAnimationsInQueue() ));
        return;
    }

    PLAY_QUEUE_ANIMATION animations[10] {};
    DBInLimit limit(10);
    int count = 0;

    if( ( count = StdSqlSelect( *APP.db,
    					Tools::format( "select %%%s from %s order by hist_an_zeit",
    							animations[0].get_table_name(),
    							animations[0].get_table_name() ),
    							DBInArrayList() >> animations, limit ) ) < 0 ) {
    	throw std::runtime_error( Tools::format( "SqlError: %s", APP.db->get_error()) );
    }

    if( count == 0 ) {
        return;
    }

    if( play_animation.countAnimationsInQueue() == 0 ) {
        play_animation.play_animation( animations[0].file() );
        CPPDEBUG( "countAnimationsInQueue: 0" );
        return;
    }


    if( count <= 1 ) {
    	return;
    }

    {
		P_PLAY_QUEUE_ANIMATION p_animation;
		p_animation = animations[0];
		p_animation.idx.data = 0;
		p_animation.setHist(BASE::HIST_TYPE::HIST_LO, "broker" );

		if( !StdSqlInsert( *APP.db, p_animation ) ) {
			CPPDEBUG( Tools::format( "cannot insert into DB: %s", APP.db->get_error() ) );
		}

	    APP.db->exec( Tools::format( "delete from %s where idx = %d",
	    		animations[0].get_table_name(), animations[0].idx() ) );

	    APP.db->commit();
    }

    play_animation.play_animation( animations[1].file() );
}

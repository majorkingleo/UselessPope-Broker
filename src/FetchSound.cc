#include "FetchSound.h"
#include "App.h"
#include "bindtypes.h"
#include <CpputilsDebug.h>

using namespace Tools;

FetchSound::FetchSound( PlaySound & ps )
: BasicThread( "FetchSound" ),
 play_sound( ps )
 {

 }

void FetchSound::run()
{
    while( !APP.quit_request ) {
        fetch_music();
        fetch_chunk();
        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
    }
}

void FetchSound::fetch_music()
{
    if( play_sound.countMusicInQueue() > 1 ) {
        return;
    }

    PLAY_QUEUE_MUSIC music {};
    DBInLimit limit{1};

    if( !StdSqlSelect( *APP.db, 
                        "select %%PLAY_QUEUE_MUSIC from PLAY_QUEUE_MUSIC order by hist_anzeit ", 
                        DBInList<DBBindType>() >> music,
                        limit ) ) {
        return;
    }
    
    play_sound.play_music( music.file() );

    APP.db->exec( Tools::format( "delete from PLAY_QUEUE_MUSIC where idx = %d", music.idx() ) );
    APP.db->commit();
}

void FetchSound::fetch_chunk()
{

}
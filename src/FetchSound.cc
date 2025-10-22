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

        APP.db->commit();

        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
    }
}

void FetchSound::fetch_music()
{
    if( play_sound.countMusicInQueue() > 1 ) {
        return;
    }

    PLAY_QUEUE_MUSIC music {};

    if( StdSqlSelect( *APP.db,
    					Tools::format( "select %%%s from %s order by hist_an_zeit ", music.get_table_name(), music.get_table_name() ),
                        DBInList<DBBindType>() >> music ) <= 0 ) {
        return;
    }
    
    play_sound.play_music( music.file() );

    APP.db->exec( Tools::format( "delete from %s where idx = %d", music.get_table_name(), music.idx() ) );
    APP.db->commit();
}

void FetchSound::fetch_chunk()
{
    PLAY_QUEUE_CHUNKS chunk {};

    if( StdSqlSelect( *APP.db,
                      Tools::format( "select %%%s from %s order by hist_an_zeit ", chunk.get_table_name(), chunk.get_table_name() ),
                      DBInList<DBBindType>() >> chunk ) <= 0 ) {
        return;
    }

    play_sound.play_chunk( chunk.file() );

    APP.db->exec( Tools::format( "delete from %s where idx = %d", chunk.get_table_name(), chunk.idx() ) );
    APP.db->commit();
}

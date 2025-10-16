#include "PlaySound.h"
#include <stdexcept>
#include <CpputilsDebug.h>
#include <format.h>
#include <chrono>
#include <thread>
#include "App.h"


using namespace Tools;
using namespace std::chrono_literals;
using namespace std::chrono;

static void init()
{
	static bool inited = false;

	if( inited ) {
		return;
	}

	inited = true;

	int result = 0;
	int flags = MIX_INIT_MP3 | MIX_INIT_OGG;

	if (SDL_Init(SDL_INIT_AUDIO) < 0) {
		throw std::runtime_error("Failed to init SDL");
	}

	if (flags != (result = Mix_Init(flags))) {
		CPPDEBUG( format("Could not initialize mixer (result: %d).\n", result) );
		throw std::runtime_error( format("Mix_Init: %s", Mix_GetError()) );
	}
}

/*
 *
 *  Music
 *
 */

PlaySound::Music::Music( const std::string & file )
: m_file( file )
{
	m_music = Mix_LoadMUS(file.c_str());

	if( !m_music ) {
		throw std::runtime_error( Tools::format( "cannot play '%s' Error: %s", m_file, Mix_GetError() ));
	}
}

PlaySound::Music::~Music()
{
	Mix_FreeMusic(m_music);
}

void PlaySound::Music::play()
{
	if( !m_started ) {
		CPPDEBUG( Tools::format( "start playing: %s", m_file ) );
		Mix_FadeInMusic(m_music,0,1000);
		m_started = true;
	}
}

bool PlaySound::Music::finished()
{
	if( !m_started ) {
		return false;
	}

	if(Mix_PlayingMusic() == 1) {
		return true;
	}

	return false;
}



/*
 *
 *  Effect
 *
 */


PlaySound::Effect::Effect( const std::string & file )
: m_file( file )
{
	m_chunk = Mix_LoadWAV(file.c_str());

	if( !m_chunk ) {
		throw std::runtime_error( Tools::format( "cannot play '%s' Error: %s", m_file, Mix_GetError() ));
	}
}

PlaySound::Effect::~Effect()
{
	Mix_FreeChunk(m_chunk);
}

void PlaySound::Effect::play()
{
	if( !m_started ) {
		CPPDEBUG( Tools::format( "start playing: %s", m_file ) );
		Mix_PlayChannel(0, m_chunk,0);
		m_started = true;
	}
}


bool PlaySound::Effect::finished()
{
	if( !m_started ) {
		return false;
	}

	if(Mix_Playing(0) == 1) {
		return true;
	}

	return false;
}

/*
 *
 *  PlaySound
 *
 */

PlaySound::PlaySound()
: BasicThread( "PlaySound" )
{
	init();

	Mix_OpenAudio(22050, AUDIO_S16SYS, 2, 640);
}

void PlaySound::play_music( const std::string & file )
{
	auto lock = std::scoped_lock( m_lock_music );
	m_music.emplace_back( file );
}

void PlaySound::play_effect( const std::string & file )
{
	auto lock = std::scoped_lock( m_lock_effects );
	m_effects.emplace_back( file );
}

void PlaySound::run()
{
	while( !APP.quit_request ) {

		{
			auto lock = std::scoped_lock( m_lock_music );
			if( !m_music.empty() ) {

				Music & music = m_music.front();
				if( music.finished() ) {
					//CPPDEBUG( "pop");
					//m_music.pop_front();
				} else {
					//CPPDEBUG( "calling play");
					music.play();
				}
			}
		}

		{
			auto lock = std::scoped_lock( m_lock_effects );
			if( !m_effects.empty() ) {

				Effect & effect = m_effects.front();
				if( effect.finished() ) {
					//CPPDEBUG( "pop");
					//m_music.pop_front();
				} else {
					//CPPDEBUG( "calling play");
					effect.play();
				}
			}
		}

		std::this_thread::sleep_for( 300ms );
	}

	CPPDEBUG( "done" );
}

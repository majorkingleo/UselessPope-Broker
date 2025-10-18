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
	CPPDEBUG( Tools::format( "finished playing: %s", m_file ) );
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

	if(Mix_PlayingMusic() == 0) {
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
		m_started_at = std::chrono::steady_clock::now();
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
					m_music.pop_front();
				} else {
					music.play();
				}
			}
		}

		{
			auto lock = std::scoped_lock( m_lock_effects );
			if( !m_effects.empty() ) {

				Effect & effect = m_effects.front();
				if( effect.finished() ) {
					m_effects.pop_front();
					continue;
				} else {
					effect.play();
				}

				const auto now = std::chrono::steady_clock::now();
				const bool current_effect_first_seconds 		= effect.get_started_at() + 5s > now;
				const bool current_effect_may_interrupted_part 	= effect.get_started_at() + 5s < now;
				const bool current_effect_is_old				= effect.get_started_at() + 30s < now;

				// drop everything within the first 5 seconds
				if( current_effect_first_seconds ) {
					while( m_effects.size() > 1 ) {

						Effect & dropping_effect = m_effects.back();
						CPPDEBUG( Tools::format( "dropping effect %s because current effect started only %d seconds ago.",
								dropping_effect.get_file(),
								duration_cast<std::chrono::seconds>(now - effect.get_started_at()).count() ));

						m_effects.pop_back();
					}
				} else if( current_effect_may_interrupted_part && m_effects.size() > 1 ) {
					m_effects.pop_front();

					CPPDEBUG( Tools::format( "dropping effect %s because current another effect got in queue.",
													effect.get_file() ));

					continue;
				} else if( current_effect_is_old ) {

					CPPDEBUG( Tools::format( "dropping effect %s because it's already played to long.",
													effect.get_file() ));

					m_effects.pop_front();
				}
			}
		}

		std::this_thread::sleep_for( 300ms );
	}

	CPPDEBUG( "done" );
}

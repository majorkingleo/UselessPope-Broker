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
 *  Chunk
 *
 */


PlaySound::Chunk::Chunk( const std::string & file )
: m_file( file )
{
	m_chunk = Mix_LoadWAV(file.c_str());

	if( !m_chunk ) {
		throw std::runtime_error( Tools::format( "cannot play '%s' Error: %s", m_file, Mix_GetError() ));
	}
}

PlaySound::Chunk::~Chunk()
{
	Mix_FreeChunk(m_chunk);
}

void PlaySound::Chunk::play()
{
	if( !m_started ) {
		m_started_at = std::chrono::steady_clock::now();
		CPPDEBUG( Tools::format( "start playing: %s", m_file ) );
		Mix_PlayChannel(0, m_chunk,0);
		m_started = true;
	}
}


bool PlaySound::Chunk::finished()
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

void PlaySound::play_chunk( const std::string & file )
{
	auto lock = std::scoped_lock( m_lock_chunks );
	m_chunks.emplace_back( file );
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
			auto lock = std::scoped_lock( m_lock_chunks );
			if( !m_chunks.empty() ) {

				Chunk & chunk = m_chunks.front();
				if( chunk.finished() ) {
					m_chunks.pop_front();
					continue;
				} else {
					chunk.play();
				}

				const auto now = std::chrono::steady_clock::now();
				const bool current_chunk_first_seconds 			= chunk.get_started_at() + 5s > now;
				const bool current_chunk_may_interrupted_part 	= chunk.get_started_at() + 5s < now;
				const bool current_chunk_is_old					= chunk.get_started_at() + 30s < now;

				// drop everything within the first 5 seconds
				if( current_chunk_first_seconds ) {
					while( m_chunks.size() > 1 ) {

						Chunk & dropping_effect = m_chunks.back();
						CPPDEBUG( Tools::format( "dropping chunk %s because current chunk started only %d seconds ago.",
								dropping_effect.get_file(),
								duration_cast<std::chrono::seconds>(now - chunk.get_started_at()).count() ));

						m_chunks.pop_back();
					}
				} else if( current_chunk_may_interrupted_part && m_chunks.size() > 1 ) {
					m_chunks.pop_front();

					CPPDEBUG( Tools::format( "dropping effect %s because current another effect got in queue.",
													chunk.get_file() ));

					continue;
				} else if( current_chunk_is_old ) {

					CPPDEBUG( Tools::format( "dropping effect %s because it's already played to long.",
													chunk.get_file() ));

					m_chunks.pop_front();
				}
			}
		}

		std::this_thread::sleep_for( 300ms );
	}

	CPPDEBUG( "done" );
}


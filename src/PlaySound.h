#pragma once

#include <string>
#include "SDL2/SDL.h"
#include "SDL2/SDL_mixer.h"
#include <thread>
#include <list>
#include <mutex>
#include "BasicThread.h"
#include <chrono>

class PlaySound : public BasicThread
{
	class Music
	{
		using duration = std::chrono::steady_clock::duration;

	private:
		std::string 		m_file				{};
		Mix_Music *			m_music				{};
		bool				m_started 			{false};

	public:
		Music( const std::string & file );
		~Music();

		void play();

		bool finished();
	};

	std::mutex 		 m_lock;

	std::list<Music> m_music;

public:

	PlaySound();

	void play_music( const std::string & file );

	void run() override;
};

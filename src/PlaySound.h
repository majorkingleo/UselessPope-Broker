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

	class Effect
	{
		using duration_t = std::chrono::steady_clock::duration;
		using time_point_t = std::chrono::steady_clock::time_point;

	private:
		std::string 		m_file				{};
		Mix_Chunk *			m_chunk				{};
		bool				m_started 			{false};
		time_point_t		m_started_at		{};

	public:
		Effect( const std::string & file );
		~Effect();

		void play();

		bool finished();

		const time_point_t & get_started_at() const {
			return m_started_at;
		}
	};

	std::mutex 		 m_lock_music;
	std::mutex 		 m_lock_effects;

	std::list<Music> 	m_music;
	std::list<Effect> 	m_effects;

public:

	PlaySound();

	void play_music( const std::string & file );
	void play_effect( const std::string & file );

	void run() override;
};

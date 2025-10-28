#pragma once

#include <string>
#include <thread>
#include <list>
#include <mutex>
#include "BasicThread.h"
#include <chrono>
#include <ConfigAnimations.h>

class PlayAnimation : public BasicThread
{
	class Animation
	{
	private:
		std::string 		m_file				{};
		bool				m_started 			{false};

	public:
		Animation( const std::string & file );
		~Animation();

		void play();

		bool finished();
	};


	mutable std::mutex 					m_lock_animation;
	std::list<Animation> 				m_animations;
	const ConfigSectionAnimations  	& 	m_cfg_animations;

public:
	PlayAnimation( const ConfigSectionAnimations & cfg_animations )
	: BasicThread( "PlayAnimation" ),
	  m_cfg_animations( cfg_animations )
	{}

	void play_animation( const std::string & file );

	void run() override;
};

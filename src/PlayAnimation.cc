#include "PlayAnimation.h"
#include "App.h"
#include <signal.h>
#include <sys/wait.h>

using namespace std::chrono_literals;

PlayAnimation::Animation::Animation( const std::string & cmd )
: m_cmd( cmd )
{

}

PlayAnimation::Animation::~Animation()
{

}

void PlayAnimation::Animation::play()
{
	if( m_pid == 0 ) {
		CPPDEBUG( Tools::format( "executing %s", m_cmd ) );
		m_pid = fork();

		if( m_pid == 0 ) {
			int ret = system( m_cmd.c_str() );
			exit(ret);
		}
	}
}

void PlayAnimation::Animation::stop()
{
	if( m_pid != 0 ) {
		kill(m_pid, SIGSTOP );
		CPPDEBUG( Tools::format( "killed %d", m_pid ) );
		int state = 0;
		waitpid(m_pid, &state, 0);
		CPPDEBUG( Tools::format( "waited"));
	}
}

void PlayAnimation::play_animation( const std::string & file )
{
	auto lock = std::scoped_lock( m_lock_animation );
	std::string cmd = Tools::format( "%s %s", m_cfg_animations.python.value, file );

	m_animations.emplace_back( cmd );
}

void PlayAnimation::run()
{
	while( !APP.quit_request ) {

		{
			auto lock = std::scoped_lock( m_lock_animation );
			if( m_animations.size() > 1 ) {
				Animation & anim = m_animations.front();
				anim.stop();
				m_animations.pop_front();
			} else if( m_animations.size() == 1 ) {
				Animation & anim = m_animations.front();
				anim.play();
			}
		}

		std::this_thread::sleep_for(100ms);
	}
}

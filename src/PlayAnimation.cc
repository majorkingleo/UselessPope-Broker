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

		APP.db.reset();

		m_pid = fork();

		if( m_pid == 0 ) {

			int fdlimit = (int)sysconf(_SC_OPEN_MAX);
			for (int i = STDERR_FILENO + 1; i < fdlimit; i++) {
				close(i);
			}

			int ret = system( m_cmd.c_str() );
			exit(ret);
		}
	}
}

void PlayAnimation::Animation::stop()
{
	if( m_pid != 0 ) {
		kill(m_pid, SIGTERM );
		CPPDEBUG( Tools::format( "killed %d", m_pid ) );
		int state = 0;
		pid_t ret;
		do {
			while( (ret = waitpid(m_pid, &state, 0) ) != m_pid ) {
				CPPDEBUG( Tools::format( Tools::format( "waiting ret: %d", m_pid ) ));
				std::this_thread::sleep_for( 100ms );
			}
			
		} while( !WIFEXITED(state) );

		CPPDEBUG( Tools::format( "waited for %d WIFEXITED: %d", m_pid, WIFEXITED(state) ));
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
		run_once();

		std::this_thread::sleep_for(100ms);
	}
}

void PlayAnimation::run_once()
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

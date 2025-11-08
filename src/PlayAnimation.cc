#include "PlayAnimation.h"
#include "App.h"
#include <signal.h>
#include <sys/wait.h>
#include <string_utils.h>
#include <CpputilsDebug.h>
#include <errno.h>
#include <string.h>

using namespace std::chrono_literals;
using namespace Tools;

PlayAnimation::Animation::Animation( const std::string & cmd )
: m_cmd( cmd )
{

}

PlayAnimation::Animation::~Animation()
{

}

extern char **environ;

void PlayAnimation::Animation::play()
{
	if( m_pid == 0 ) {
		CPPDEBUG( Tools::format( "executing %s", m_cmd ) );

		APP.db.dispose();

		m_pid = fork();

		if( m_pid == 0 ) {

			// but python might ignore this
			signal(SIGCHLD, SIG_DFL);

			// close all file descriptors, except stdin, stdout, stderr
			int fdlimit = (int)sysconf(_SC_OPEN_MAX);
			for (int i = STDERR_FILENO + 1; i < fdlimit; i++) {
				close(i);
			}

			auto sl = split_string( m_cmd, " " );

			std::vector<char*> args;
			for( auto & s: sl ) {
				args.push_back( const_cast<char*>( s.c_str() ) );
			}
			args.push_back( NULL );

			int ret = execve( args[0], &args[0], environ );
			CPPDEBUG( Tools::format( "execve returned %d %s", ret, strerror(errno) ) );
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
		bool done = false;
		do {
			while( (ret = waitpid(m_pid, &state, 0) ) != m_pid ) {
				if( ret == -1 && errno == ECHILD ) {
					done = true;
					break;
				}
				CPPDEBUG( Tools::format( Tools::format( "waiting ret: %d m_pid: %d error: %s", ret, m_pid, strerror(errno) ) ));
				std::this_thread::sleep_for( 100ms );
			}
			
		} while( !done && !WIFEXITED(state) );

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

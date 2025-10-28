#include "PlayAnimation.h"
#include "App.h"

using namespace std::chrono_literals;

PlayAnimation::Animation::Animation( const std::string & file )
: m_file( file )
{

}

PlayAnimation::Animation::~Animation()
{

}

void PlayAnimation::Animation::play()
{

}

void PlayAnimation::play_animation( const std::string & file )
{

}

void PlayAnimation::run()
{
	while( !APP.quit_request ) {

		std::this_thread::sleep_for(100ms);
	}
}

#include "ConfigGlobal.h"
#include "cppdir.h"
#include "ConfigAnimations.h"

using namespace Tools;

Configfile2::SectionKey<ConfigSectionAnimations> ConfigSectionAnimations::KEY( "animations" );


ConfigSectionAnimations::ConfigSectionAnimations( const std::string & name_, Configfile2 *config_file_ )
: Section( name_, config_file_ ),
  python( "python", "/home/papst/ypy/bin/python" )
{
	registerValue( &python );
}

void ConfigSectionAnimations::registerSection( Configfile2 *config_file )
{
	config_file->registerSection( new ConfigSectionAnimations(ConfigSectionAnimations::KEY.name, config_file ) );
}









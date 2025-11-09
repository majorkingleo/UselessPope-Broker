#include "ConfigGlobal.h"
#include "cppdir.h"
#include "ConfigPopeReacts.h"

using namespace Tools;

Configfile2::SectionKey<ConfigSectionPopeReacts> ConfigSectionPopeReacts::KEY( "pope_reacts" );


ConfigSectionPopeReacts::ConfigSectionPopeReacts( const std::string & name_, Configfile2 *config_file_ )
: Section( name_, config_file_ ),
  answers( "answers", "/home/papst/UselessPope-Broker/papst_answers.txt" )
{
	registerValue( &answers );
}

void ConfigSectionPopeReacts::registerSection( Configfile2 *config_file )
{
	config_file->registerSection( new ConfigSectionPopeReacts(ConfigSectionPopeReacts::KEY.name, config_file ) );
}









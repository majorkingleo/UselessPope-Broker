#include "ConfigGlobal.h"
#include "cppdir.h"
#include "ConfigDatabase.h"

using namespace Tools;

Configfile2::SectionKey<ConfigSectionDatabase> ConfigSectionDatabase::KEY( "database" );


ConfigSectionDatabase::ConfigSectionDatabase( const std::string & name_, Configfile2 *config_file_ )
: Section( name_, config_file_ ),
  Host( "host", "localhost" ),
  UserName( "username", "broker" ),
  Password( "password", "broker" ),
  Instance( "instance", "pope" )
{
	registerValue( &Host );
	registerValue( &UserName );
	registerValue( &Password );
	registerValue( &Instance );
}

void ConfigSectionDatabase::registerSection( Configfile2 *config_file )
{
	config_file->registerSection( new ConfigSectionDatabase(ConfigSectionDatabase::KEY.name, config_file ) );
}









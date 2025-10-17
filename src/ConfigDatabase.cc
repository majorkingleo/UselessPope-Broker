#include "ConfigGlobal.h"
#include "cppdir.h"
#include "ConfigDatabase.h"

using namespace Tools;

Configfile2::SectionKey<ConfigSectionDatabase> ConfigSectionDatabase::KEY( "database" );


ConfigSectionDatabase::ConfigSectionDatabase( const std::string & name_, Configfile2 *config_file_ )
: Section( name_, config_file_ ),
  UserName( "username", "broker" ),
  Password( "password", "broker" )
{
	registerValue( &UserName );
	registerValue( &Password );
}

void ConfigSectionDatabase::registerSection( Configfile2 *config_file )
{
	config_file->registerSection( new ConfigSectionDatabase(ConfigSectionDatabase::KEY.name, config_file ) );
}









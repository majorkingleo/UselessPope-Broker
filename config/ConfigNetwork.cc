#include "ConfigGlobal.h"
#include "cppdir.h"
#include "ConfigNetwork.h"

using namespace Tools;

Configfile2::SectionKey<ConfigSectionNetwork> ConfigSectionNetwork::KEY( "network" );


ConfigSectionNetwork::ConfigSectionNetwork( const std::string & name_, Configfile2 *config_file_ )
: Section( name_, config_file_ ),
  UDPListenPort( "UDPListenPort", 22000 ),
  BF1942DiscoverPort( "BF1942DiscoverPort", 54915 ),
  BF1942DiscoverEnabled( "BF1942DiscoverEnabled", true )
{
	registerValue( &UDPListenPort );
	registerValue( &BF1942DiscoverPort );
	registerValue( &BF1942DiscoverEnabled );
}

void ConfigSectionNetwork::registerSection( Configfile2 *config_file )
{
	config_file->registerSection( new ConfigSectionNetwork(ConfigSectionNetwork::KEY.name, config_file ) );
}









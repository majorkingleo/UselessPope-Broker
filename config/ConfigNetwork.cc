#include "ConfigGlobal.h"
#include "cppdir.h"
#include "ConfigNetwork.h"

using namespace Tools;

Configfile2::SectionKey<ConfigSectionNetwork> ConfigSectionNetwork::KEY( "network" );


ConfigSectionNetwork::ConfigSectionNetwork( const std::string & name_, Configfile2 *config_file_ )
: Section( name_, config_file_ ),
  UDPListenPort( "UDPListenPort", 22000 ),
  BF1942DiscoverPort( "BF1942DiscoverPort", 54915 ),
  BF1942DiscoverEnabled( "BF1942DiscoverEnabled", true ),
  BF1942QueryEnabled( "BF1942QueryEnabled", true ),
  BF1942QueryPort( "BF1942QueryPort", 22000 ),
  BF1942QueryInterval( "BF1942QueryInterval", 15 ),
  BF1942QueryTimeout( "BF1942QueryTimeout", 3 ),
  BF1942QueryHosts( "BF1942QueryHosts", "" )
{
	registerValue( &UDPListenPort );
	registerValue( &BF1942DiscoverPort );
	registerValue( &BF1942DiscoverEnabled );
	registerValue( &BF1942QueryEnabled );
	registerValue( &BF1942QueryPort );
	registerValue( &BF1942QueryInterval );
	registerValue( &BF1942QueryTimeout );
	registerValue( &BF1942QueryHosts );
}

void ConfigSectionNetwork::registerSection( Configfile2 *config_file )
{
	config_file->registerSection( new ConfigSectionNetwork(ConfigSectionNetwork::KEY.name, config_file ) );
}









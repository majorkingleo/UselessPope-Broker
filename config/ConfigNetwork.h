#pragma once

#include "Configfile2.h"

class ConfigSectionNetwork : public Configfile2::Section
{
public:
	static Configfile2::SectionKey<ConfigSectionNetwork> KEY;

	CONFIG_SIMPLE_DECLARE_INT( UDPListenPort );
	CONFIG_SIMPLE_DECLARE_INT( BF1942DiscoverPort );
	CONFIG_SIMPLE_DECLARE_BOOL( BF1942DiscoverEnabled );
	CONFIG_SIMPLE_DECLARE_BOOL( BF1942QueryEnabled );
	CONFIG_SIMPLE_DECLARE_INT( BF1942QueryPort );
	CONFIG_SIMPLE_DECLARE_INT( BF1942QueryInterval );
	CONFIG_SIMPLE_DECLARE_INT( BF1942QueryTimeout );
	CONFIG_SIMPLE_DECLARE_STR( BF1942QueryHosts );

public:
	ConfigSectionNetwork( const std::string & name_, Configfile2 *config_file_ );

	static void registerSection( Configfile2 *config_file );
};



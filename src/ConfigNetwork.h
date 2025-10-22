#pragma once

#include "Configfile2.h"

class ConfigSectionNetwork : public Configfile2::Section
{
public:
	static Configfile2::SectionKey<ConfigSectionNetwork> KEY;

	CONFIG_SIMPLE_DECLARE_INT( UDPListenPort );

public:
	ConfigSectionNetwork( const std::string & name_, Configfile2 *config_file_ );

	static void registerSection( Configfile2 *config_file );
};



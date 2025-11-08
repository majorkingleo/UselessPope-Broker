#pragma once

#include "Configfile2.h"

class ConfigSectionPopeReacts : public Configfile2::Section
{
public:
	static Configfile2::SectionKey<ConfigSectionPopeReacts> KEY;

	CONFIG_SIMPLE_DECLARE_STR( answers );

public:
	ConfigSectionPopeReacts( const std::string & name_, Configfile2 *config_file_ );

	static void registerSection( Configfile2 *config_file );
};



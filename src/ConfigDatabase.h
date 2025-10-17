#pragma once

#include "Configfile2.h"

class ConfigSectionDatabase : public Configfile2::Section
{
public:
	static Configfile2::SectionKey<ConfigSectionDatabase> KEY;

	CONFIG_SIMPLE_DECLARE_STR( UserName );
	CONFIG_SIMPLE_DECLARE_STR( Password );


public:
	ConfigSectionDatabase( const std::string & name_, Configfile2 *config_file_ );

	static void registerSection( Configfile2 *config_file );
};



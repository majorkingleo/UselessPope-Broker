#pragma once

#include "Configfile2.h"

class ConfigSectionAnimations : public Configfile2::Section
{
public:
	static Configfile2::SectionKey<ConfigSectionAnimations> KEY;

	CONFIG_SIMPLE_DECLARE_STR( python );

public:
	ConfigSectionAnimations( const std::string & name_, Configfile2 *config_file_ );

	static void registerSection( Configfile2 *config_file );
};



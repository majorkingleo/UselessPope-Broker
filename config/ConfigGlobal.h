#pragma once

#include "Configfile2.h"

class ConfigSectionGlobal : public Configfile2::Section
{
public:
	static Configfile2::SectionKey<ConfigSectionGlobal> KEY;

protected:


public:
	ConfigSectionGlobal( const std::string & name_, Configfile2 *config_file_ );

	static void registerSection( Configfile2 *config_file );

};



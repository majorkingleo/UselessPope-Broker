/*
 * ConfigBattery.h
 *
 *  Created on: 07.01.2022
 *      Author: martin
 */

#ifndef LIBWIDGETS_CONFIGBATTERY_H_
#define LIBWIDGETS_CONFIGBATTERY_H_

#include "Configfile2.h"

class ConfigSectionBattery : public Configfile2::Section
{
public:
	static Configfile2::SectionKey<ConfigSectionBattery> KEY;

	CONFIG_SIMPLE_DECLARE_STR( IconDir );
	CONFIG_SIMPLE_DECLARE_STR( Icon );
	CONFIG_SIMPLE_DECLARE_STR( ScriptDir );
	CONFIG_SIMPLE_DECLARE_STR( ScriptName );
	CONFIG_SIMPLE_DECLARE_STR( OutFileName );


public:
	ConfigSectionBattery( const std::string & name_, Configfile2 *config_file_ );

	virtual void read( Tools::Leo::Ini & ini );

	static void registerSection( Configfile2 *config_file );
};



#endif /* LIBWIDGETS_CONFIGBATTERY_H_ */

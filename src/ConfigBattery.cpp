/*
 * ConfigBattery.cpp
 *
 *  Created on: 07.01.2022
 *      Author: martin
 */
#include "ConfigBattery.h"
#include "ConfigGlobal.h"
#include "ConfigOutput.h"
#include "cppdir.h"

using namespace Tools;

Configfile2::SectionKey<ConfigSectionBattery> ConfigSectionBattery::KEY( "battery" );


ConfigSectionBattery::ConfigSectionBattery( const std::string & name_, Configfile2 *config_file_ )
: Section( name_, config_file_ ),
  IconDir( "batteryicondir", "" ),
  Icon( "batteryicon", "" ),
  ScriptDir( "batteryscriptdir", "" ),
  ScriptName( "batteryscriptname", "" ),
  OutFileName( "batteryoutfilename", "" )
{
	registerValue( &IconDir );
	registerValue( &Icon );
	registerValue( &ScriptDir );
	registerValue( &ScriptName );
	registerValue( &OutFileName );
}

void ConfigSectionBattery::read( Tools::Leo::Ini & ini )
{
	Configfile2::Section::read(ini);


	// Icon
	createRealtiveOrAbsolutPathFor( (*config_file)[ConfigSectionGlobal::KEY].getIconDirectory(),
									IconDir );

	Icon.value = CppDir::concat_dir( IconDir, Icon );

	DEBUG( format( "%s/%s=%s", name, Icon.name, Icon.value ));


	// Script
	createRealtiveOrAbsolutPathFor( (*config_file)[ConfigSectionGlobal::KEY].getScriptDirectory(),
									ScriptDir );

	ScriptName.value = CppDir::concat_dir( ScriptDir, ScriptName );

	DEBUG( format( "%s/%s=%s", name, ScriptName.name, ScriptName.value ));


	// Output
	createRealtiveOrAbsolutPathFor( (*config_file)[ConfigSectionOutput::KEY].getOutPutDir(),
									OutFileName );

}


void ConfigSectionBattery::registerSection( Configfile2 *config_file )
{
	config_file->registerSection( new ConfigSectionBattery(ConfigSectionBattery::KEY.name, config_file ) );
}









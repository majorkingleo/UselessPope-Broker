/*
 * ConfigGlobal.cpp
 *
 *  Created on: 07.01.2022
 *      Author: martin
 */
#include "ConfigGlobal.h"
#include "cppdir.h"
#include <unistd.h>
#include <pwd.h>

using namespace Tools;

Configfile2::SectionKey<ConfigSectionGlobal> ConfigSectionGlobal::KEY( "global" );

static std::string defaultPic2DisplayPath()
{
	std::string path = getpwuid(getuid())->pw_dir;
	path = CppDir::concat_dir( path, "/smartpic/monitor/src/pic2display/pic2display" );
	return path;
}

ConfigSectionGlobal::ConfigSectionGlobal( const std::string & name_, Configfile2 *config_file_ )
: Section( name_, config_file_ ),
  iconDirectory( "icondir", "" ),
  scriptDirectory("scriptdir", ""),
  displayType("displaytype", "EPD_7in5b_V2" ),
  pic2dispPath("pic2disppath", defaultPic2DisplayPath() )
{
	registerValue( &iconDirectory );
	registerValue( &scriptDirectory );
	registerValue( &displayType );
	registerValue( &pic2dispPath );
}

std::string ConfigSectionGlobal::getIconDirectory() const {
	return Configfile2::makePathsAbsolute(iconDirectory);
}

std::string ConfigSectionGlobal::getScriptDirectory() const {
	return Configfile2::makePathsAbsolute(scriptDirectory);
}

std::string ConfigSectionGlobal::getDisplayType() const {
	return displayType;
}

std::string ConfigSectionGlobal::getPic2DispPath() const {
	return Configfile2::makePathsAbsolute(pic2dispPath);
}

void ConfigSectionGlobal::registerSection( Configfile2 *config_file )
{
	config_file->registerSection( new ConfigSectionGlobal(ConfigSectionGlobal::KEY.name, config_file ) );
}

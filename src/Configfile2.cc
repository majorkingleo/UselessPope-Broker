
#include "Configfile2.h"
#include <unistd.h>
#include <pwd.h>
#include <cppdir.h>
#include "ConfigOutput.h"
#include "ConfigWeather.h"
#include "ConfigGlobal.h"
#include "ConfigClockDigital.h"
#include "ConfigLastUpdated.h"
#include "ConfigBattery.h"
#include "ConfigPubTransport.h"
#include "ConfigCalendar.h"

using namespace Tools;
using namespace Leo;

void Configfile2::Section::read( Tools::Leo::Ini & ini ) {
	for( Value *value : values ) {
		value->read( ini, name );
	}
}

Configfile2::Section::~Section() {
	for( Value *value : values ) {
		delete value;
	}

	values.clear();
}

Configfile2::Section::Section( const std::string & name_, Configfile2 *config_file_ )
: name( name_ ),
  values(),
  config_file( config_file_ )
{

}

void Configfile2::Section::createRealtiveOrAbsolutPathFor( const std::string & IconDir,  ValueType<std::string> & Icon )
{
	std::string icon_abs_path =  Configfile2::makePathsAbsolute(Icon);

	if( icon_abs_path.find("/") != 0 ) {
		icon_abs_path = CppDir::concat_dir(IconDir,icon_abs_path);
	}

	Icon.value = icon_abs_path;

	DEBUG( format( "%s/%s=%s", name, Icon.name, Icon.value ));
}

Configfile2* Configfile2::instance = NULL;

Configfile2* Configfile2::getIncance()
{
	return instance;
}

Configfile2* Configfile2::createInstance( const std::string & config_file )
{
	if( instance ) {
		delete instance;
	}

	instance = new Configfile2( config_file );

	return instance;
}

Configfile2::Configfile2( const std::string & config_file_ )
: config_file( config_file_ ),
  sections(),
  sections_in_init_order()
{

}

Configfile2::~Configfile2()
{
	for( auto sections_pair : sections ) {
		delete sections_pair.second;
	}

	sections.clear();
	sections_in_init_order.clear();
}

std::string Configfile2::makePathsAbsolute(const std::string & sIn) {

	std::string retPath = "";
	if (sIn.substr(0,1) == "~") {
		std::string path = getpwuid(getuid())->pw_dir;
		retPath = substitude(sIn, "~", path, 0);
	} else {
		retPath = sIn;
	}

	//std::cout << "Got |" << sIn << "| Return --> |" << retPath << "|" << std::endl;

	return retPath;
}

void Configfile2::read( bool autoUpdateIniFile )
{
	/* Call leoini */
	std::string cfgFile = makePathsAbsolute(config_file);

	int mode = std::ios_base::in;

	if( autoUpdateIniFile ) {
		mode |=  std::ios_base::out;
	}

	Ini ini (cfgFile, mode );

	if (!ini) {
		throw STDERR_EXCEPTION( format( "Error opening config-file '%s'", cfgFile ) );
	} else {
		DEBUG( format( "Config file '%s' successfully opened.", cfgFile ) );
	}

	// official comment sign is only ';'
	ini.add_comment_sign( "#" );

	for( auto section : sections_in_init_order ) {
		section->read( ini );
	}

	if( autoUpdateIniFile ) {
		ini.write();
	}
}

void Configfile2::registerSection( Section * section ) {
	sections[section->getName()] = section;
	sections_in_init_order.push_back( section );
}

Configfile2* Configfile2::createDefaultInstaceWithAllModules( const std::string & config_file )
{
	Configfile2* instance = createInstance( config_file );

	ConfigSectionGlobal::registerSection( instance );
	ConfigSectionWeather::registerSection( instance );
	ConfigSectionOutput::registerSection( instance );
	ConfigSectionClockDigital::registerSection( instance );
	ConfigSectionLastUpdated::registerSection( instance );
	ConfigSectionBattery::registerSection( instance );
	ConfigSectionPubTransport::registerSection( instance );
	ConfigSectionCalendar::registerSection( instance );

	return instance;
}




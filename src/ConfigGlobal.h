/*
 * ConfigGlobal.h
 *
 *  Created on: 07.01.2022
 *      Author: martin
 */

#ifndef LIBWIDGETS_CONFIGGLOBAL_H_
#define LIBWIDGETS_CONFIGGLOBAL_H_

#include "Configfile2.h"

class ConfigSectionGlobal : public Configfile2::Section
{
public:
	static Configfile2::SectionKey<ConfigSectionGlobal> KEY;

protected:
	ValueType<std::string> iconDirectory;
	ValueType<std::string> scriptDirectory;
	ValueType<std::string> displayType;
	ValueType<std::string> pic2dispPath;

public:
	ConfigSectionGlobal( const std::string & name_, Configfile2 *config_file_ );

	std::string getIconDirectory() const;
	std::string getScriptDirectory() const;
	std::string getDisplayType() const;
	std::string getPic2DispPath() const;

	static void registerSection( Configfile2 *config_file );

};



#endif /* LIBWIDGETS_CONFIGGLOBAL_H_ */

#pragma once

#include <tuple>
#include <string>
#include <leoini.h>
#include <CpputilsDebug.h>
#include <string_utils.h>
#include <stderr_exception.h>
#include <map>

#define CONFIG_SIMPLE_DECLARE_STR( name ) \
	ValueType<std::string> name; \
	std::string get##name() const { \
		return name; \
	}

#define CONFIG_SIMPLE_DECLARE_INT( name ) \
	ValueType<int> name; \
	int get##name() const { \
		return name; \
	}

#define CONFIG_SIMPLE_DECLARE_BOOL( name ) \
	ValueType<bool> name; \
	bool is##name() const { \
		return name; \
	}

class Configfile2
{
public:
	template<typename T> class SectionKey
	{
	public:
		typedef T TYPE;

	public:
		const std::string name;

		SectionKey( const std::string & name_ )
		: name( name_ )
		{}
	};

	class Section
	{
	protected:
		class Value
		{
		public:
			const std::string name;
			bool mandatory;
			bool found_in_config_file;

		public:
			Value( const std::string & name_ )
			: name( name_ ),
			  mandatory( false ),
			  found_in_config_file( false )
			{}

			virtual ~Value() {}
			virtual void read( Tools::Leo::Ini & ini, const std::string & section ) = 0;


		protected:
			void getTvalue( const std::string & in, std::string & out )
			{
				out = in;
			}

			void getTvalue( const std::string & in, bool & out )
			{
				out = Tools::s2bool(in);
			}

			template<typename T> void getTvalue( const std::string & in, T & out )
			{
				out = Tools::s2x<T>( in );
			}

			template<typename T=std::string> T readIniValue( Tools::Leo::Ini & ini, const std::string & key, const std::string & value, const std::string & default_value )
			{
				Tools::Leo::Ini::Element c_ele (Tools::Leo::Ini::Element::TYPE::KEY, key, value, default_value );
				if (!ini.read(c_ele)) {

					if( !default_value.empty() ) {
						CPPDEBUG( Tools::format( "Value <%s> not found! using default: %s",
								c_ele.key,
								c_ele.value) );

						ini.write( c_ele );

						T ret;
						getTvalue( default_value, ret );
						return ret;
					}

					CPPDEBUG( Tools::format( "Value <%s> not found!", c_ele.key) );
				} else {
					found_in_config_file = true;
				}

				T ret;
				getTvalue( c_ele.value, ret );

				CPPDEBUG( Tools::format( "Ini: %s/%s=%s => %s", c_ele.section, c_ele.key, c_ele.value, ret ));

				return ret;
			}

		}; // class Value

		template<typename T> class ValueType : public Value
		{
		public:
			T value;
			T default_value;

		public:
			ValueType( const std::string & name_, const T & default_value_ )
			: Value( name_ ),
			  default_value( default_value_ )
			{}

			virtual void read( Tools::Leo::Ini & ini, const std::string & section )
			{
				value = readIniValue<T>( ini, section, name, Tools::x2s( default_value ) );
			}

			operator const T&() const {
				return value;
			}

		}; // class ValueType

	protected:
		const std::string name;
		std::list<Value*> values;
		Configfile2 *config_file;

	public:
		Section( const std::string & name_, Configfile2 *config_file );

		virtual ~Section();

		const std::string getName() const {
			return name;
		}

		virtual void read( Tools::Leo::Ini & ini );

	protected:
		// values won't be deleted
		void registerValue( Value * value ) {
			values.push_back( value );
		}

		/*
		 * If Icon.value already has an absolut path, nothing will happen
		 * If Icon.value starts with ~/ it will be expandet to /home/xxx/
		 * In any other case the icon_dir will be prefix to Icon.value: Icon.value = icon_dir + Icon.value
		 */
		void createRealtiveOrAbsolutPathFor( const std::string & icon_dir, ValueType<std::string> & Icon );

	};

protected:
	std::string config_file;
	std::map<std::string,Section*> sections;

	// call the read method in the order the sections where
	// registerd. Using the map, would result in a kind of alphabetic order.
	std::list<Section*> sections_in_init_order;

	static Configfile2* instance;

public:
	Configfile2( const std::string & cfgFile_ = "~/.homemon.cfg" );
	~Configfile2();

	void read( bool autoUpdateIniFile = false );

	void registerSection( Section * section );

	template<typename T> const T & operator[]( const SectionKey<T> & key ) {
		T* section = dynamic_cast<typename SectionKey<T>::TYPE*>(sections[key.name]);

		if( section == NULL ) {
			throw STDERR_EXCEPTION( Tools::format( "no section named '%s' registered", key.name ) );
		}

		return *section;
	}

	template<typename T> const T & at( const SectionKey<T> & key ) {
		T* section = dynamic_cast<typename SectionKey<T>::TYPE*>(sections[key.name]);

		if( section == NULL ) {
			throw STDERR_EXCEPTION( Tools::format( "no section named '%s' registered", key.name ) );
		}

		return *section;
	}

	template<typename T> static const T & get( const SectionKey<T> & key ) {
		return getIncance()->at(key);
	}

	static std::string makePathsAbsolute(const std::string & sIn);

	static Configfile2* getIncance();
	static Configfile2* createInstance( const std::string & config_file = "~/.UselessPope-Broker.ini" );
	static Configfile2* createDefaultInstaceWithAllModules( const std::string & config_file = "~/.UselessPope-Broker.ini" );
};



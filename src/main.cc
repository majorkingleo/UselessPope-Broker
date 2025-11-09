#include <iostream>
#include <arg.h>
#include <OutDebug.h>
#include <DetectLocale.h>
#include <format.h>
#include "PlaySound.h"
#include "App.h"
#include <filesystem>
#include "Configfile2.h"
#include "ConfigDatabase.h"
#include "ConfigNetwork.h"
#include "ConfigAnimations.h"
#include "ConfigPopeReacts.h"
#include "bindtypes.h"
#include <dbi.h>
#include "FetchSound.h"
#include "ButtonListener.h"
#include "FetchButton.h"
#include <thread>
#include <chrono>
#include "PlayAnimation.h"
#include "FetchAnimation.h"
#include "FetchAnswers.h"
#include "FetchStats.h"

using namespace Tools;
using namespace std::chrono_literals;

class Co : public ColoredOutput
{
public:
	std::string good( const std::string & message )
	{
		return color_output( ColoredOutput::GREEN, message );
	}

	std::string bad( const std::string & message )
	{
		return color_output( ColoredOutput::BRIGHT_RED, message );
	}
};

static void usage( const std::string & prog )
{
	  std::cerr << "usage: "
				<< prog << "\n";
}

namespace {

template <class TABLE>
void insert_TABLE( const std::string & name, const std::string & key, const std::string & value )
{
	TABLE existing_table;

	CPPDEBUG( Tools::format( "%s key=%s value=%s", name, key.c_str(), value.c_str() ) );

	if( StdSqlSelect( *APP.db,
			Tools::format( "select %%%s from %s where `%s` = '%s' ",
					existing_table.get_table_name(),
					existing_table.get_table_name(),
					existing_table.key.get_name(),
					escape( key ) ),
			DBInList<DBBindType>() >> existing_table ) > 0 ) {
		return;
	}

	TABLE table {};
	table.setHist( BASE::HIST_TYPE::HIST_AN, "broker" );
	table.setHist( BASE::HIST_TYPE::HIST_AE, "broker" );
	table.setHist( BASE::HIST_TYPE::HIST_LO, "broker" );

	table.key = key;
	table.value = value;

	if( !StdSqlInsert( *APP.db, table ) ) {
		CPPDEBUG( Tools::format( "cannot insert into DB: %s", APP.db->get_error() ) );
	}

	APP.db->commit();
}

static void insert_config( const std::string & key, const std::string & value )
{
	insert_TABLE<CONFIG>( "config", key, value );
}

static void insert_stats( const std::string & key, const std::string & value )
{
	insert_TABLE<STATS>( "stats", key, value );
}

} // namespace

static void insert_default_values()
{
	insert_config( "brightness", "0.02" );
	insert_config( "current_animation", "0" );
	insert_config( "fog", "0" );
	insert_config( "animation0", "/home/papst/UselessPope-raspi/python/pope_default_rotating_color_wheel.py" );
	insert_config( "animation1", "/home/papst/UselessPope-raspi/python/pope_red_eyes.py" );

	insert_stats( FetchStats::MOST_PLAYED_SOUND, "" );
	insert_stats( FetchStats::ACTIVE_LEDS_DATA, "0123" );
	insert_stats( FetchStats::RPMS, "1234" );
	insert_stats( FetchStats::FREQUENCY, "73" );
	insert_stats( FetchStats::TOP_USER_1, "der.mucki (97 actions)");
	insert_stats( FetchStats::TOP_USER_2, "der.mucki (97 actions)");
	insert_stats( FetchStats::TOP_USER_3, "der.mucki (97 actions)");
	insert_stats( FetchStats::TOTAL_ACTIONS, "666" );
}

int main( int argc, char **argv )
{
	Co co;

	std::list<std::thread> threads;

	try {
		Arg::Arg arg( argc, argv );

		arg.addPrefix( "-" );
		arg.addPrefix( "--" );

		arg.setMinMatch(1);

		Arg::OptionChain oc_info;
		arg.addChainR( &oc_info );
		oc_info.setMinMatch( 1 );
		oc_info.setContinueOnMatch( false );
		oc_info.setContinueOnFail( true );

		Arg::FlagOption o_help( "h" );
		o_help.addName( "help" );
		o_help.setDescription( "Show this page" );
		oc_info.addOptionR( &o_help );

		Arg::FlagOption o_version( "v" );
		o_version.addName( "version" );
		o_version.setDescription( "Show replace version number" );
		oc_info.addOptionR( &o_version );


		Arg::OptionChain oc_main;
		arg.addChainR( &oc_main );
		oc_main.setMinMatch( 0 );
		oc_main.setContinueOnMatch( true );
		oc_main.setContinueOnFail( true );

		Arg::FlagOption o_debug("debug");
		o_debug.setDescription("print debugging messages");
		o_debug.setRequired(false);
		oc_main.addOptionR( &o_debug );

		Arg::StringOption o_enqueue_music("enqueue-music");
		o_enqueue_music.setDescription("add music to queue");
		o_enqueue_music.setRequired(false);
		o_enqueue_music.setMinValues(1);
		oc_main.addOptionR( &o_enqueue_music );

		Arg::FlagOption o_create_sql("create-sql");
		o_create_sql.setDescription("print create sql script");
		o_create_sql.setRequired(false);
		oc_main.addOptionR( &o_create_sql );

		Arg::FlagOption o_with_drop_table("with-drop-table");
		o_with_drop_table.setDescription("add drop table statements");
		o_with_drop_table.setRequired(false);
		oc_main.addOptionR( &o_with_drop_table );

		Arg::StringOption o_enqueue_chunk("enqueue-chunk");
		o_enqueue_chunk.setDescription("add a chunk file to queue");
		o_enqueue_chunk.setRequired(false);
		o_enqueue_chunk.setMinValues(1);
		o_enqueue_chunk.setMaxValues(1);
		oc_main.addOptionR( &o_enqueue_chunk );


		Arg::StringOption o_enqueue_animation("enqueue-animation");
		o_enqueue_animation.setDescription("add a animation file to queue");
		o_enqueue_animation.setRequired(false);
		o_enqueue_animation.setMinValues(1);
		o_enqueue_animation.setMaxValues(1);
		oc_main.addOptionR( &o_enqueue_animation );

		DetectLocale dl;

		const unsigned int console_width = 80;

		if( !arg.parse() )
		{
			std::cout << "failed\n";

			if( o_version.getState() )
			{
				std::cout << format("%s version %s\n", argv[0], VERSION);
				return 0;
			} else {
				usage(argv[0]);
				std::cout << arg.getHelp(5,20,30, console_width ) << std::endl;
				return 1;
			}
		}

		if( o_help.getState() ) {
			usage(argv[0]);
			std::cout << arg.getHelp(5,20,30, console_width ) << std::endl;
			return 0;
		}


		if( o_debug.getState() ) {
			Tools::x_debug = new OutDebug();
		}

		if( o_create_sql.getState() ) {			
			std::cout << create_sql( o_with_drop_table.getState() ) << std::endl;
		}

		Configfile2::createDefaultInstaceWithAllModules()->read(true);
		const ConfigSectionDatabase 	& cfg_db 			= Configfile2::get(ConfigSectionDatabase::KEY);
		const ConfigSectionNetwork  	& cfg_net 			= Configfile2::get(ConfigSectionNetwork::KEY);
		const ConfigSectionPopeReacts  	& cfg_pope_reacts	= Configfile2::get(ConfigSectionPopeReacts::KEY);
	
		std::chrono::steady_clock::time_point retry_logon_until{};

		retry_logon_until = std::chrono::steady_clock::now() + std::chrono::seconds(cfg_db.retry_db_timeout.value);


		while( !APP.db ) {

			APP.reconnect_db = [&cfg_db]() {
				APP.db.connect( cfg_db.Host,
								cfg_db.UserName,
								cfg_db.Password,
								cfg_db.Instance,
								Database::DB_MYSQL );
			};

			APP.reconnect_db();

			if( !APP.db->valid() ) {
				if( retry_logon_until < std::chrono::steady_clock::now() ) {
					throw STDERR_EXCEPTION( Tools::format( "cannot connect to database: '%s'", APP.db->get_error()));
				} else {
					CPPDEBUG( Tools::format( "cannot connect to database: '%s' retrying...", APP.db->get_error()) );
					APP.db.dispose();
					std::this_thread::sleep_for(500ms);
					continue;
				}
			}
		}

		if( o_enqueue_chunk.isSet() ) {
			for( const auto & file : *o_enqueue_chunk.getValues() ) {
				if( !std::filesystem::exists(file) ) {
					throw STDERR_EXCEPTION( Tools::format( "file '%s' does not exists", file ) );
				}

				PLAY_QUEUE_CHUNKS pqc {};
				pqc.file = std::filesystem::absolute( file ).string();
				pqc.setHist(BASE::HIST_TYPE::HIST_AN);
				pqc.setHist(BASE::HIST_TYPE::HIST_AE);
				pqc.setHist(BASE::HIST_TYPE::HIST_LO);

				if( !StdSqlInsert( *APP.db, pqc ) ) {
					throw STDERR_EXCEPTION( Tools::format( "cannot enqueue file '%s' '%s'", file, APP.db->get_error() ) );
				}
				APP.db->commit();
			}

			return 0;
		}

		if( o_enqueue_music.isSet() ) {
			for( const auto & file : *o_enqueue_music.getValues() ) {
				if( !std::filesystem::exists(file) ) {
					throw STDERR_EXCEPTION( Tools::format( "file '%s' does not exists", file ) );
				}

				PLAY_QUEUE_MUSIC pqm {};
				pqm.file = std::filesystem::absolute( file ).string();
				pqm.setHist(BASE::HIST_TYPE::HIST_AN);
				pqm.setHist(BASE::HIST_TYPE::HIST_AE);
				pqm.setHist(BASE::HIST_TYPE::HIST_LO);

				if( !StdSqlInsert( *APP.db, pqm ) ) {
					throw STDERR_EXCEPTION( Tools::format( "cannot enqueue file '%s' '%s'", file, APP.db->get_error() ) );
				}
				APP.db->commit();
			}

			return 0;
		}

		if( o_enqueue_animation.isSet() ) {
			for( const auto & file : *o_enqueue_animation.getValues() ) {
				if( !std::filesystem::exists(file) ) {
					throw STDERR_EXCEPTION( Tools::format( "file '%s' does not exists", file ) );
				}

				PLAY_QUEUE_ANIMATION pqa {};
				pqa.file = std::filesystem::absolute( file ).string();
				pqa.setHist(BASE::HIST_TYPE::HIST_AN);
				pqa.setHist(BASE::HIST_TYPE::HIST_AE);
				pqa.setHist(BASE::HIST_TYPE::HIST_LO);

				if( !StdSqlInsert( *APP.db, pqa ) ) {
					throw STDERR_EXCEPTION( Tools::format( "cannot enqueue file '%s' '%s'", file, APP.db->get_error() ) );
				}
				APP.db->commit();
			}

			return 0;
		}

		insert_default_values();

		if( !std::filesystem::exists(cfg_pope_reacts.answers.value) ) {
			throw STDERR_EXCEPTION( Tools::format( "file '%s' does not exists", cfg_pope_reacts.answers.value ) );
		}


		threads.emplace_back([&cfg_pope_reacts]() {
			FetchAnswers answers {};
			answers.fetch_from_file( cfg_pope_reacts.answers.value );
			auto token = APP.db.get_dispose_token();
			answers.run();
		});

		threads.emplace_back([&cfg_net]() {
			ButtonListener listener( cfg_net.UDPListenPort );
			auto token = APP.db.get_dispose_token();
			listener.run();
		});

		threads.emplace_back([]() {
			FetchButton button_worker;
			auto token = APP.db.get_dispose_token();
			button_worker.run();
		});


		PlaySound play_sound {};

		threads.emplace_back([&play_sound]() {
			auto token = APP.db.get_dispose_token();
			play_sound.run();
		});

		threads.emplace_back([&play_sound]() {
			auto token = APP.db.get_dispose_token();
			FetchSound fetch( play_sound );
			fetch.run();
		});


		const ConfigSectionAnimations  	& cfg_animations 	= Configfile2::get(ConfigSectionAnimations::KEY);

		PlayAnimation 	play_animation {cfg_animations};

		threads.emplace_back([&play_animation]() {
			auto token = APP.db.get_dispose_token();
			play_animation.run();
		});

		threads.emplace_back([&play_animation]() {
			auto token = APP.db.get_dispose_token();
			FetchAnimation 	fetch( play_animation );
			fetch.run();
		});


		threads.emplace_back([]() {
			auto token = APP.db.get_dispose_token();
			FetchStats 	stats {};
			stats.run();
		});

		while (!SDL_QuitRequested()) {
			SDL_Delay(250);
		}

		APP.quit_request = true;

		for( auto & t : threads ) {
			t.join();
		}

		return 0;


	} catch( std::exception & err ) {
		std::cerr << co.bad("error: ") << err.what() << std::endl;
		return 10;
	}

	return 0;
}

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

	CPPDEBUG( Tools::format( "%s key=%s value=%s", key.c_str(), value.c_str() ) );

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
	insert_config( "animation0", "/home/papst/UselessPope-raspi/python/pope_default_rotating_color_wheel.py" );
	insert_config( "animation1", "/home/papst/UselessPope-raspi/python/pope_red_eyes.py" );

	insert_stats( "mostplayedsound", "" );
	insert_stats( "active-leds-data", "0123" );
	insert_stats( "umdrehungen", "1234" );
	insert_stats( "frequenz", "73" );
	insert_stats( "user1", "der.mucki (97 actions)");
	insert_stats( "totalactions", "666" );
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

		Arg::FlagOption o_debug("debug");
		o_debug.setDescription("print debugging messages");
		o_debug.setRequired(false);
		arg.addOptionR( &o_debug );

		Arg::StringOption o_enqueue_music("enqueue-music");
		o_enqueue_music.setDescription("add music to queue");
		o_enqueue_music.setRequired(false);
		o_enqueue_music.setMinValues(1);
		arg.addOptionR( &o_enqueue_music );

		Arg::FlagOption o_create_sql("create-sql");
		o_create_sql.setDescription("print create sql script");
		o_create_sql.setRequired(false);
		arg.addOptionR( &o_create_sql );

		Arg::FlagOption o_with_drop_table("with-drop-table");
		o_with_drop_table.setDescription("add drop table statements");
		o_with_drop_table.setRequired(false);
		arg.addOptionR( &o_with_drop_table );

		Arg::StringOption o_enqueue_chunk("enqueue-chunk");
		o_enqueue_chunk.setDescription("add a chunk file to queue");
		o_enqueue_chunk.setRequired(false);
		o_enqueue_chunk.setMinValues(1);
		o_enqueue_chunk.setMaxValues(1);
		arg.addOptionR( &o_enqueue_chunk );


		Arg::StringOption o_enqueue_animation("enqueue-animation");
		o_enqueue_animation.setDescription("add a animation file to queue");
		o_enqueue_animation.setRequired(false);
		o_enqueue_animation.setMinValues(1);
		o_enqueue_animation.setMaxValues(1);
		arg.addOptionR( &o_enqueue_animation );

		Arg::FlagOption o_master("master");
		o_master.setDescription("execute as master daemon program");
		o_master.setRequired(false);
		arg.addOptionR( &o_master );

		Arg::FlagOption o_listen("listen");
		o_listen.setDescription("listen to buttons");
		o_listen.setRequired(false);
		arg.addOptionR( &o_listen );


		Arg::FlagOption o_button_worker("button-worker");
		o_button_worker.setDescription("work on button pressed events");
		o_button_worker.setRequired(false);
		arg.addOptionR( &o_button_worker );

		Arg::IntOption o_db_retry_logon("retry-db-timeout");
		o_db_retry_logon.setDescription("try relogin on DB for XX seconds");
		o_db_retry_logon.setRequired(false);
		o_db_retry_logon.setMaxValues(1);
		o_db_retry_logon.setMinValues(1);
		arg.addOptionR( &o_db_retry_logon );

		Arg::FlagOption o_master_animations("master-animations");
		o_master_animations.setDescription("execute current animations from db");
		o_master_animations.setRequired(false);
		arg.addOptionR( &o_master_animations );

		Arg::StringOption o_pope_answers_file("pope-reacts-answers-file");
		o_pope_answers_file.setDescription("pop reactions answers file");
		o_pope_answers_file.setRequired(false);
		o_pope_answers_file.setMinValues(1);
		arg.addOptionR( &o_pope_answers_file );

		Arg::FlagOption o_master_stats("master-stats");
		o_master_stats.setDescription("calculate statistics server");
		o_master_stats.setRequired(false);
		arg.addOptionR( &o_master_stats );

		DetectLocale dl;

		const unsigned int console_width = 80;

		if( !arg.parse() || argc <= 1 )
		{
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
	
		std::chrono::steady_clock::time_point retry_logon_until{};

		if( o_db_retry_logon.isSet() ) {
			 unsigned sec = s2x<unsigned>((o_db_retry_logon.getValues()->at(0)), 0 );
			 retry_logon_until = std::chrono::steady_clock::now() + std::chrono::seconds(sec);
		}

		while( !APP.db ) {

			APP.reconnect_db = [&cfg_db]() {
				APP.db = std::make_shared<Database>( cfg_db.Host,
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
					APP.db.reset();
					std::this_thread::sleep_for(500ms);
					continue;
				}
			}

			insert_default_values();
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
		}

		if( o_pope_answers_file.isSet() ) {

			FetchAnswers answers {};

			for( const auto & file : *o_pope_answers_file.getValues() ) {
				if( !std::filesystem::exists(file) ) {
					throw STDERR_EXCEPTION( Tools::format( "file '%s' does not exists", file ) );
				}

				answers.fetch_from_file( file );
			}
		}

		bool does_something = false;

		if( o_listen.isSet() ) {
			ButtonListener listener( cfg_net.UDPListenPort );
			listener.run();
			return 0;
		}

		if( o_button_worker.isSet() ) {
			FetchButton button_worker;
			button_worker.run();
			return 0;
		}

		if( o_master.isSet() ) {
			PlaySound play {};
			FetchSound fetch( play );
			//ButtonListener listener( cfg_net.UDPListenPort );

			threads.emplace_back([&play]() {
				play.run();
			});

			threads.emplace_back([&fetch]() {
				fetch.run();
			});

			/*
			threads.emplace_back([&listener]() {
				listener.run();
			});*/

			while (!SDL_QuitRequested()) {
				SDL_Delay(250);
			}

			APP.quit_request = true;

			for( auto & t : threads ) {
				t.join();
			}
		}

		if( o_master_animations.isSet() ) {

			const ConfigSectionAnimations  	& cfg_animations 	= Configfile2::get(ConfigSectionAnimations::KEY);

			PlayAnimation 	play {cfg_animations};
			FetchAnimation 	fetch( play );
/*
			threads.emplace_back([&play]() {
				play.run();
			});
*/

			threads.emplace_back([&fetch]() {
				fetch.run();
			});

			while (!SDL_QuitRequested()) {
				SDL_Delay(250);
			}

			APP.quit_request = true;

			for( auto & t : threads ) {
				t.join();
			}
		}

		if( o_master_stats.isSet() ) {
			FetchStats 	stats {};

			threads.emplace_back([&stats]() {
				stats.run();
			});

			while (!SDL_QuitRequested()) {
				SDL_Delay(250);
			}

			APP.quit_request = true;

			for( auto & t : threads ) {
				t.join();
			}
		}

	} catch( std::exception & err ) {
		std::cerr << co.bad("error: ") << err.what() << std::endl;
		return 10;
	}

	return 0;
}

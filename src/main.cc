#include <iostream>
#include <arg.h>
#include "logging.h"
#include <ColoredOutput.h>
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
#include "Bf1942Discover.h"
#include "Bf1942Query.h"
#include "FetchButton.h"
#include <thread>
#include <chrono>
#include "PlayAnimation.h"
#include "FetchAnimation.h"
#include "FetchAnswers.h"
#include "FetchStats.h"
#include "AutoStopFog.h"
#include "ResolveUsernames.h"
#include "UsernameResolver.h"

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
struct insert_TABLE_ret
{
	bool inserted {};
	TABLE table {};
};

template <class TABLE>
insert_TABLE_ret<TABLE> insert_TABLE( const std::string & name, const std::string & key, const std::string & value )
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
		return { false, existing_table };
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

	return { true, table };
}

static void insert_config( const std::string & key, const std::string & value, bool force = false )
{
	auto ret = insert_TABLE<CONFIG>( "config", key, value );

	if( !force ) {
		return;
	}

	if( !ret.inserted ) {

		CONFIG & config = ret.table;
		config.setHist( BASE::HIST_TYPE::HIST_AE, "broker" );
		config.value.data = value;
		if( StdSqlUpdate( *APP.db, ret.table, Tools::format( "where `%s` = '%d'", config.idx.get_name(), config.idx.data )) ) {
			APP.db->commit();
		}
	}
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
	insert_config( "fog", "0", true );

	insert_config( "animation0", "/home/papst/UselessPope-raspi/python/pope_default_rotating_color_wheel.py" );
	insert_config( "animation1", "/home/papst/UselessPope-raspi/python/pope_red_eyes.py" );
	insert_config( "animation2", "/home/papst/UselessPope-raspi/python/matrix_rain.py" );
	insert_config( "animation3", "/home/papst/UselessPope-raspi/python/snake_duel.py" );
	insert_config( "animation4", "/home/papst/UselessPope-raspi/python/screen_off.py" );

	insert_config( "animation0_title", "Johannes Paul II" );
	insert_config( "animation1_title", "Johannes Diabolo III" );
	insert_config( "animation2_title", "Matrix" );
	insert_config( "animation3_title", "Snake" );
	insert_config( "animation4_title", "Screen Off" );


	insert_stats( FetchStats::MOST_PLAYED_SOUND, "" );
	insert_stats( FetchStats::ACTIVE_LEDS_DATA, "0123" );
	insert_stats( FetchStats::RPMS, "0" );
	insert_stats( FetchStats::FREQUENCY, "73" );
	insert_stats( FetchStats::TOP_USER_1, "der.mucki (97 actions)");
	insert_stats( FetchStats::TOP_USER_2, "der.mucki (97 actions)");
	insert_stats( FetchStats::TOP_USER_3, "der.mucki (97 actions)");
	insert_stats( FetchStats::TOTAL_ACTIONS, "666" );
	insert_stats( FetchStats::SNAKE_P1_WINS, "0" );
	insert_stats( FetchStats::SNAKE_P2_WINS, "0" );
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

		Arg::StringOption o_log_file("log-file");
		o_log_file.setDescription("append debug output to a log file");
		o_log_file.setRequired(false);
		o_log_file.setMinValues(1);
		o_log_file.setMaxValues(1);
		oc_main.addOptionR( &o_log_file );

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

		Arg::FlagOption o_resolve_usernames("resolve-usernames");
		o_resolve_usernames.setDescription("fill missing usernames in archived chunks and SERMON");
		o_resolve_usernames.setRequired(false);
		oc_main.addOptionR( &o_resolve_usernames );

		Arg::StringOption o_query_bf1942("query-bf1942");
		o_query_bf1942.setDescription("query a BF1942 server (HOST[:PORT]) and print the scoreboard");
		o_query_bf1942.setRequired(false);
		o_query_bf1942.setMinValues(1);
		o_query_bf1942.setMaxValues(1);
		oc_main.addOptionR( &o_query_bf1942 );

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


		const std::string log_file = o_log_file.isSet() && !o_log_file.getValues()->empty()
			? o_log_file.getValues()->front()
			: broker_log_path( "broker.log" );

		logInit( log_file, o_debug.getState() );
		LogSession log_session;

		if( o_create_sql.getState() ) {			
			std::cout << create_sql( o_with_drop_table.getState() ) << std::endl;
			return 0;
		}

		Configfile2::createDefaultInstaceWithAllModules()->read(true);
		const ConfigSectionDatabase 	& cfg_db 			= Configfile2::get(ConfigSectionDatabase::KEY);
		const ConfigSectionNetwork  	& cfg_net 			= Configfile2::get(ConfigSectionNetwork::KEY);
		const ConfigSectionPopeReacts  	& cfg_pope_reacts	= Configfile2::get(ConfigSectionPopeReacts::KEY);
	
		if( o_query_bf1942.isSet() ) {
			std::string spec = o_query_bf1942.getValues()->front();
			std::string host = spec;
			int port = cfg_net.BF1942QueryPort;

			const std::string::size_type colon = spec.rfind( ':' );
			if( colon != std::string::npos ) {
				host = spec.substr( 0, colon );
				try {
					port = std::stoi( spec.substr( colon + 1 ) );
				} catch( ... ) {
					port = cfg_net.BF1942QueryPort;
				}
			}

			const bool ok = Bf1942Query::query_once( host, port, cfg_net.BF1942QueryTimeout );
			return ok ? 0 : 1;
		}

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

		if( o_resolve_usernames.getState() ) {
			resolve_protocol_usernames();
			return 0;
		}

		if( o_enqueue_chunk.isSet() ) {
			for( const auto & file : *o_enqueue_chunk.getValues() ) {
				if( !std::filesystem::exists(file) ) {
					throw STDERR_EXCEPTION( Tools::format( "file '%s' does not exists", file ) );
				}

				const std::string abs_file = std::filesystem::absolute( file ).string();
				const std::string username = UsernameResolver::from_file( abs_file );

				PLAY_QUEUE_CHUNKS pqc {};
				pqc.file = abs_file;
				pqc.setHist(BASE::HIST_TYPE::HIST_AN, username);
				pqc.setHist(BASE::HIST_TYPE::HIST_AE, username);
				pqc.setHist(BASE::HIST_TYPE::HIST_LO, username);

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

				const std::string abs_file = std::filesystem::absolute( file ).string();
				const std::string username = UsernameResolver::from_file( abs_file );

				PLAY_QUEUE_MUSIC pqm {};
				pqm.file = abs_file;
				pqm.setHist(BASE::HIST_TYPE::HIST_AN, username);
				pqm.setHist(BASE::HIST_TYPE::HIST_AE, username);
				pqm.setHist(BASE::HIST_TYPE::HIST_LO, username);

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

				const std::string abs_file = std::filesystem::absolute( file ).string();
				const std::string username = UsernameResolver::from_file( abs_file );

				PLAY_QUEUE_ANIMATION pqa {};
				pqa.file = abs_file;
				pqa.setHist(BASE::HIST_TYPE::HIST_AN, username);
				pqa.setHist(BASE::HIST_TYPE::HIST_AE, username);
				pqa.setHist(BASE::HIST_TYPE::HIST_LO, username);

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

		threads.emplace_back([&cfg_net]() {
			if( !cfg_net.isBF1942DiscoverEnabled() ) {
				return;
			}
			auto token = APP.db.get_dispose_token();
			Bf1942Discover discover( cfg_net.BF1942DiscoverPort );
			discover.run();
		});

		threads.emplace_back([&cfg_net]() {
			if( !cfg_net.isBF1942QueryEnabled() ) {
				return;
			}
			auto token = APP.db.get_dispose_token();
			Bf1942Query query( cfg_net.BF1942QueryPort,
			                   cfg_net.BF1942QueryInterval,
			                   cfg_net.BF1942QueryTimeout,
			                   cfg_net.BF1942QueryHosts );
			query.run();
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

		threads.emplace_back([]() {
			auto token = APP.db.get_dispose_token();
			AutoStopFog auto_stop_fog {};
			auto_stop_fog.run();
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

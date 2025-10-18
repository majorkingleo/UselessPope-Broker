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
#include "bindtypes.h"
#include <dbi.h>

using namespace Tools;

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

int main( int argc, char **argv )
{
	Co co;

	std::optional<PlaySound> play;

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

		Arg::StringOption o_play("play");
		o_play.setDescription("play a file");
		o_play.setRequired(false);
		o_play.setMinValues(1);
		arg.addOptionR( &o_play );

		Arg::FlagOption o_create_sql("create-sql");
		o_create_sql.setDescription("print create sql script");
		o_create_sql.setRequired(false);
		arg.addOptionR( &o_create_sql );

		Arg::StringOption o_play_chunk("play-chunk");
		o_play_chunk.setDescription("add a chunk file to queue");
		o_play_chunk.setRequired(false);
		o_play_chunk.setMinValues(1);
		o_play_chunk.setMaxValues(1);
		arg.addOptionR( &o_play_chunk );

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
			std::cout << create_sql() << std::endl;
		}

		Configfile2::createDefaultInstaceWithAllModules()->read(true);
		const ConfigSectionDatabase & cfg_db = Configfile2::get(ConfigSectionDatabase::KEY);

		APP.db = std::make_shared<Database>( cfg_db.Host,
											 cfg_db.UserName,
											 cfg_db.Password,
											 cfg_db.Instance,
											 Database::DB_MYSQL );

		if( !APP.db->valid() ) {
			throw STDERR_EXCEPTION( Tools::format( "cannot connect to database: '%s'", APP.db->get_error()));
		}

		if( o_play_chunk.isSet() ) {
			for( const auto & file : *o_play_chunk.getValues() ) {
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

		bool does_something = false;

		if( o_play.isSet() ) {
			play.emplace();

			for( const auto & file : *o_play.getValues() ) {
				play->play_music( file );
			}

			does_something = true;
		}

		if( does_something ) {

			if( play ) {
				threads.emplace_back([&play]() {
					play->run();
				});
			}

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

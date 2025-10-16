#include <iostream>
#include <arg.h>
#include <OutDebug.h>
#include <DetectLocale.h>
#include <format.h>
#include "PlaySound.h"
#include "App.h"
#include <filesystem>

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

		if( o_help.getState() )
		{
			usage(argv[0]);
			std::cout << arg.getHelp(5,20,30, console_width ) << std::endl;
			return 0;
		}


		if( o_debug.getState() )
		{
			Tools::x_debug = new OutDebug();
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

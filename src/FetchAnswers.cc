#include "FetchAnswers.h"
#include <fstream>
#include <read_file.h>
#include <CpputilsDebug.h>
#include <format.h>
#include <string_utils.h>
#include <utf8_util.h>
#include <chrono>
#include "App.h"
#include <thread>

using namespace Tools;
using namespace std::chrono_literals;
using namespace std::chrono;

FetchAnswers::Reaction::Reaction( const std::wstring & song_title )
: m_song_title( song_title )
{
	// ./UT3/announcer/UT3 announcer - monsterkill.ogg

	// 1) remove .ogg
	strip_file_extension( m_song_title );

	// 2) remove leading path
	strip_file_name_from_path( m_song_title );

	// 3) extract keywords
	m_key_words = get_key_words_from_title( m_song_title );

	CPPDEBUG( Tools::wformat( L"song: %s keywords: %s", m_song_title,
			IterableToFormattedWString( m_key_words.get_key_words() ) ) );
}

std::set<std::wstring> FetchAnswers::Reaction::get_key_words_from_title( const std::wstring & title )
{
	std::set<std::wstring> key_words;
	std::vector<std::wstring> words = split_and_strip_simple( title, L" \t\r-_," );

//	CPPDEBUG( Tools::wformat( L"words(%d): %s", words.size(), IterableToFormattedWString( words ) ) );

	for( std::wstring word : words ) {
		word = substitude( word, L"'", L"" );
		word = tolower( word );
		key_words.insert( std::move(word) );
	}

	return key_words;
}

void FetchAnswers::Reaction::strip_file_extension( std::wstring & file_name )
{
	std::wstring::size_type pos = file_name.find_last_of(L"/.");

	if( pos == std::wstring::npos ) {
		return;
	}

	if( file_name[pos] == L'/' || pos == 0 ) {
		return;
	}

	file_name = file_name.substr( 0, pos );
}

void FetchAnswers::Reaction::strip_file_name_from_path( std::wstring & file_name )
{
	std::wstring::size_type pos = file_name.find_last_of(L"/");

	if( pos == std::wstring::npos ) {
		return;
	}

	file_name = file_name.substr( pos + 1 );

	file_name = strip( file_name, L"/");
}

std::wstring FetchAnswers::Reaction::strip_file_name( const std::wstring & file_name )
{
	std::wstring ret = file_name;
	strip_file_extension( ret );
	strip_file_name_from_path( ret );

	return ret;
}

FetchAnswers::FetchAnswers()
: BasicThread( "FetchAnswers" )
{

}

void FetchAnswers::run()
{
	const auto timeout = 1min;
	auto deadline = steady_clock::now();

    while( !APP.quit_request ) {

    	if( steady_clock::now() > deadline ) {
    		APP.db->commit();

    		try {

    			fetch_last_played_chunks();

    		} catch( const std::exception & error ) {
    			CPPDEBUG( Tools::format( "Error: %s", error.what() ));
    		}

    		APP.db->commit();
    		deadline = steady_clock::now() + timeout;
    	}

        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
    }
}

void FetchAnswers::fetch_from_file( const std::string & file_name )
{
	ReadFile read_file {};
	std::wstring content;
	if( !read_file.read_file( file_name, content ) )  {
		throw std::runtime_error( Tools::format( "cannot read file: %s", read_file.getError() ) );
	}

	std::vector<std::wstring> lines = split_and_strip_simple( content, L"\n" );

	std::optional<Reaction> current_reaction {};

	m_reactions.reserve(lines.size()/2);

	for( unsigned i = 0; i < lines.size(); ++i )
	{
		const std::wstring & line = lines[i];

		if( line.empty() ) {
			continue;
		}

		if( !is_space( line[0] ) ) {
			// new song title
			if( current_reaction ) {
				m_reactions.push_back( *current_reaction );
			}

			current_reaction.emplace( line );

		} else {

			if( !current_reaction ) {
				CPPDEBUG( Tools::format( "%s:%d Ignoring answer without leading song title at the line before.", file_name, i+1 ) );
				continue;
			}

			current_reaction->add_answer( line );
		}
	}
}

void FetchAnswers::get_reaction_from_song( const std::string & file )
{
	Reaction current_title( Utf8Util::utf8toWString( file ) );
	const KeyWords & current_title_key_words = current_title.get_key_words();

	unsigned best_match {};
	std::vector<Reaction*> best_reactions {};

	for( Reaction & reaction : m_reactions ) {
		unsigned match = reaction.get_key_words().match( current_title_key_words );

		if( match > best_match ) {
			best_reactions.clear();
			best_match = match;
			best_reactions.push_back( &reaction );
		} else if( match == best_match ) {
			best_reactions.push_back( &reaction );
		}
	}

	for( Reaction *reaction : best_reactions ) {
		CPPDEBUG( Tools::wformat( L"(%d) song: %s, pope reacts: %s",
				best_match,
				current_title.get_title(),
				reaction->get_answer() ) );
	}
}

void FetchAnswers::fetch_last_played_chunks()
{

}

#include "FetchAnswers.h"
#include <fstream>
#include <read_file.h>
#include <CpputilsDebug.h>
#include <format.h>
#include <string_utils.h>

using namespace Tools;

FetchAnswers::Reaction::Reaction( const std::wstring & song_title )
: m_song_title( song_title )
{
	// ./UT3/announcer/UT3 announcer - monsterkill.ogg

	// 1) remove .ogg
	strip_file_extension( m_song_title );

	// 2) remove leading path
	strip_file_name_from_path( m_song_title );

	CPPDEBUG( Tools::wformat( L"%s", m_song_title ) );
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

	file_name = file_name.substr( 0, pos - 1);
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

FetchAnswers::FetchAnswers()
: BasicThread( "FetchAnswers" )
{

}

void FetchAnswers::run()
{

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

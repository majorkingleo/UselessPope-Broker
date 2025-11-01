#include "FetchAnswers.h"
#include <fstream>
#include <read_file.h>
#include <CpputilsDebug.h>
#include <format.h>
#include <string_utils.h>

FetchAnswers::FetchAnswers()
: BasicThread( "FetchAnswers" )
{

}

void FetchAnswers::fetch_from_file( const std::string & file_name )
{
	ReadFile read_file {};
	std::wstring content;
	if( !read_file.read_file( file_name, content ) )  {
		throw std::runtime_error( Tools::format( "cannot read file: %s", read_file.getError() ) );
	}

	std::vector<std::wstring> sl = split_and_strip_simple( )
}

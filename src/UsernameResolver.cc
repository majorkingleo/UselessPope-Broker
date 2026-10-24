#include "UsernameResolver.h"

#include <filesystem>
#include <cstddef>
#include <string>

#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

constexpr std::size_t USER_NAME_MAX = 20;
const std::string UNKNOWN_USER = "unbekannt";

std::string owner_of( const std::filesystem::path & path )
{
	struct stat st {};

	if( ::stat( path.c_str(), &st ) != 0 ) {
		return {};
	}

	struct passwd * pw = ::getpwuid( st.st_uid );

	if( !pw || !pw->pw_name ) {
		return {};
	}

	return pw->pw_name;
}

std::string owner_of_existing_parent( const std::filesystem::path & file )
{
	std::filesystem::path path = file;
	std::error_code ec;

	while( !path.empty() && !std::filesystem::exists( path, ec ) ) {
		path = path.parent_path();
	}

	if( path.empty() ) {
		return {};
	}

	return owner_of( path );
}

std::string username_from_home_path( const std::string & file )
{
	static const std::string prefix = "/home/";

	if( file.compare( 0, prefix.size(), prefix ) != 0 ) {
		return {};
	}

	const auto end = file.find( '/', prefix.size() );

	if( end == std::string::npos || end == prefix.size() ) {
		return {};
	}

	return file.substr( prefix.size(), end - prefix.size() );
}

} // namespace

namespace UsernameResolver {

std::string from_file( const std::string & file )
{
	if( file.empty() ) {
		return UNKNOWN_USER;
	}

	std::string username = username_from_home_path( file );

	if( username.empty() ) {
		username = owner_of_existing_parent( file );
	}

	if( username.empty() ) {
		username = UNKNOWN_USER;
	}

	if( username.size() > USER_NAME_MAX ) {
		username.resize( USER_NAME_MAX );
	}

	return username;
}

std::string resolve( const std::string & file, const std::string & username )
{
	if( !username.empty() ) {
		return username;
	}

	return from_file( file );
}

} // namespace UsernameResolver

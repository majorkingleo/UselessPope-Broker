#pragma once

#include <string>

namespace UsernameResolver {

// Returns the best available actor name for a queue row that has no
// hist_*_user set. The file owner is used as a fallback, because entries
// enqueued by cron (the clock) or by the bare `broker -enqueue-*` command do
// not carry a username. If the file no longer exists, the owner of the closest
// existing parent directory is used.
std::string from_file( const std::string & file );

// Keeps an already known username, otherwise resolves it from the file.
std::string resolve( const std::string & file, const std::string & username );

} // namespace UsernameResolver

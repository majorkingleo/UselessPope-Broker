#pragma once

#include <string>
#include <CpputilsDebug.h>

// Start the debug logging session (broker port of showfavicon-win's logging).
//
// `logFile`  path of the log file; empty disables the file backend.
// `console`  mirrors every message to stdout. The broker is a console daemon,
//            so `-debug` enables this (same as before, but now async).
//
// Call once, before anything can emit a CPPDEBUG message, and rely on
// LogSession (below) - or an explicit logShutdown() - so the last lines drain
// and the logger thread is joined before the process exits.
void logInit( const std::string & logFile, bool console );
void logShutdown();

// Returns <home>/.broker/<file_name>, creating the ~/.broker directory on first
// use (falls back to ./.broker if the home directory cannot be determined).
// Used for the main log and for per-subsystem logs (e.g. bf-network.log).
std::string broker_log_path( const std::string & file_name );

// RAII: shuts the logger down on scope exit, so an early return or an
// exception from main() still drains and joins the logger thread (a joinable
// std::thread left behind would call std::terminate at static destruction).
class LogSession
{
public:
	LogSession() = default;
	~LogSession() { logShutdown(); }

	LogSession( const LogSession & ) = delete;
	LogSession & operator=( const LogSession & ) = delete;
};

#include "AsyncFileLogger.h"
#include <stderr_exception.h>
#include <utf8_util.h>
#include <filesystem>
#include <format.h>
#include <ctime>
#include <chrono>
#include <system_error>

using namespace AsyncOut;
using namespace Tools;

FileLogger::FileLogger( const std::string & filename_, std::uintmax_t max_size_ )
: m_filename(filename_),
  m_max_size(max_size_),
  m_written(0)
{
    m_file.open( m_filename, std::ios::out | std::ios::app );

    if( !m_file ) {
        throw STDERR_EXCEPTION( Tools::format( "cannot open log file '%s'", m_filename ) );
    }

    // Account for an existing file, so the cap also holds across restarts.
    std::error_code ec;
    const auto size = std::filesystem::file_size( m_filename, ec );
    if( !ec ) {
        m_written = size;
    }
}

void FileLogger::rotate()
{
    m_file.close();

    std::error_code ec;
    std::filesystem::rename( m_filename, m_filename + ".1", ec );

    // POSIX rename() replaces an existing target; this is only a fallback for
    // platforms where it does not (and if the rename genuinely failed).
    if( ec ) {
        std::filesystem::remove( m_filename + ".1", ec );
        std::filesystem::rename( m_filename, m_filename + ".1", ec );
    }

    m_file.open( m_filename, std::ios::out | std::ios::trunc );
    m_written = 0;
}

void FileLogger::log()
{
    auto lock = std::lock_guard(m_file_mutex);

	auto all_messages = popAll();

	for( const auto & m : all_messages ) {

		// localtime_r() honours the TZ environment variable; gcc 12 (bookworm)
		// has no std::chrono time-zone database, so the reference's
		// time_zone + std::format path is replaced by this.
		const std::time_t tt = std::chrono::system_clock::to_time_t( m.when );
		std::tm tm {};
		localtime_r( &tt, &tm );

		char timestamp[32];
		std::strftime( timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm );

		std::string message;

		if( const auto str_ptr (std::get_if<std::string>(&m.message)); str_ptr) {
			message = *str_ptr;
		} else {
			message = Utf8Util::wStringToUtf8(std::get<std::wstring>(m.message));
		}

		// Build the whole line first, so the size check is exact.
		std::string line;
		line += '[';
		line += timestamp;
		line += "] ";
		line += std::filesystem::path(m.file).filename().string();
		line += ':';
		line += std::to_string(m.line);
		line += ' ';
		line += message;
		line += '\n';

		// Rotate before writing if this line would push the file over the cap.
		// (m_max_size == 0 means "no limit".)
		if( m_max_size > 0 && m_written > 0 && m_written + line.size() > m_max_size ) {
			rotate();
		}

		m_file << line;
		m_written += line.size();
	}
}

/**
 * async file backend for CPPDEBUG
 * @author Copyright (c) 2023 Martin Oberzalek
 *
 * Broker adaptation: the reference (showfavicon-win) stamps each line via
 * std::chrono::time_zone + std::format; gcc 12 has neither, so the timestamp is
 * produced with localtime_r() + strftime() in the .cc instead (localtime_r
 * honours the TZ environment variable).
 */
#pragma once

#include "AsyncOutDebug.h"
#include <fstream>
#include <mutex>

namespace AsyncOut {

class FileLogger : public AsyncOut::Logger
{
    std::string                             m_filename;
    std::ofstream                           m_file;
    std::mutex                              m_file_mutex;

public:
    FileLogger( const std::string & filename_ );

    void log() override;

    void flush() {
        auto lock = std::lock_guard(m_file_mutex);
        m_file.flush();
    }
};

} // namespace AsyncOut

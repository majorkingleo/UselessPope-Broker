/**
 * async file backend for CPPDEBUG
 * @author Copyright (c) 2023 Martin Oberzalek
 *
 * Broker adaptation: the reference (showfavicon-win) stamps each line via
 * std::chrono::time_zone + std::format; gcc 12 has neither, so the timestamp is
 * produced with localtime_r() + strftime() in the .cc instead (localtime_r
 * honours the TZ environment variable).
 *
 * The file is capped: when appending a line would push it past `max_size`, the
 * current file is rotated to `<file>.1` (the previous `.1` is replaced) and a
 * fresh file is started. The active log therefore never exceeds `max_size`.
 */
#pragma once

#include "AsyncOutDebug.h"
#include <fstream>
#include <mutex>
#include <cstdint>

namespace AsyncOut {

inline constexpr std::uintmax_t kDefaultMaxLogSize = 10 * 1024 * 1024;  // 10 MB

class FileLogger : public AsyncOut::Logger
{
    std::string                             m_filename;
    std::ofstream                           m_file;
    std::mutex                              m_file_mutex;
    std::uintmax_t                          m_max_size;
    std::uintmax_t                          m_written;

public:
    explicit FileLogger( const std::string & filename_,
                         std::uintmax_t max_size_ = kDefaultMaxLogSize );

    void log() override;

    void flush() {
        auto lock = std::lock_guard(m_file_mutex);
        m_file.flush();
    }

private:
    void rotate();
};

} // namespace AsyncOut

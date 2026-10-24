#include "logging.h"

// cpputils debug plumbing (see src/tools_config.h of the reference project).
#include "AsyncFileLogger.h"
#include "AsyncOutDebug.h"

#include "diag.h"

#include <CpputilsDebug.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <pthread.h>

namespace {

// How long the logger thread idles when nothing arrives.
constexpr std::chrono::milliseconds kIdleTimeout{200};

// The file is flushed on a timer, not per line - same cadence the reference uses.
constexpr std::chrono::seconds kFlushInterval{3};

std::unique_ptr<AsyncOut::Debug> g_frontend;
std::unique_ptr<AsyncOut::FileLogger> g_file;
std::unique_ptr<AsyncOut::Logger> g_console;
std::thread g_thread;
std::atomic<bool> g_quit{false};

// A backend must never be able to kill this thread: an exception escaping it
// would call std::terminate and take the whole process with it. Both backends
// convert wide text, which can throw, so swallowing here costs the batch the
// backend had popped - not the process, and not all further logging.
void drainConsole() {
    if (!g_console) {
        return;
    }
    try {
        g_console->log();
    } catch (...) {
    }
}

void drainFile() {
    if (!g_file) {
        return;
    }
    try {
        g_file->log();
    } catch (...) {
    }
}

void flushFile() {
    if (!g_file) {
        return;
    }
    try {
        g_file->flush();
    } catch (...) {
    }
}

// The backend loop. A backend's semaphore is released by every message, so
// waiting on one of them is enough to know that anything arrived. On Linux
// std::counting_semaphore really blocks (futex), so this is the correct wake
// path - the reference keeps the MinGW busy-spin workaround only for MinGW.
void run() {
    pthread_setname_np(pthread_self(), "logger");

    // Identifiable as "logger" in the debugger, next to the worker threads.
    // LOG_TIMING only: reports how often this loop turned and the CPU it used.
    Tools::Heartbeat heartbeat("logger");

    // The backend to wait on.
    AsyncOut::Logger* primary = g_file
                                    ? static_cast<AsyncOut::Logger*>(g_file.get())
                                    : g_console.get();

    auto next_flush = std::chrono::steady_clock::now() + kFlushInterval;

    while (!g_quit) {
        heartbeat.tick();

        drainConsole();
        drainFile();

        if (std::chrono::steady_clock::now() > next_flush) {
            flushFile();
            next_flush = std::chrono::steady_clock::now() + kFlushInterval;
        }

        // The logger wakes as soon as a backend is handed a message, instead
        // of on the next poll.
        if (primary) {
            primary->wait_for(kIdleTimeout);
        } else {
            std::this_thread::sleep_for(kIdleTimeout);
        }
    }

    // Final drain, while the backends are still alive.
    drainConsole();
    drainFile();
}

}  // namespace

void logInit( const std::string & logFile, bool console ) {
    if (!logFile.empty()) {
        // Backends before the frontend: a log file that cannot be opened must
        // not leave Tools::x_debug installed pointing at a half-built object.
        try {
            g_file = std::make_unique<AsyncOut::FileLogger>(logFile);
        } catch (const std::exception&) {
            g_file.reset();
        }
    }

    if (console) {
        g_console = std::make_unique<AsyncOut::Logger>();
    }

    if (!g_file && !g_console) {
        return;  // nowhere to deliver to; CPPDEBUG stays a no-op
    }

    g_frontend = std::make_unique<AsyncOut::Debug>();

    if (g_file) {
        g_frontend->subscribe(g_file.get());
    }
    if (g_console) {
        g_frontend->subscribe(g_console.get());
    }

    // From here on CPPDEBUG has somewhere to go.
    Tools::x_debug = g_frontend.get();

    g_quit = false;
    g_thread = std::thread(run);
}

void logShutdown() {
    g_quit = true;

    if (g_thread.joinable()) {
        g_thread.join();
    }

    // Stop any further CPPDEBUG before the subscriber list is dismantled.
    Tools::x_debug = nullptr;

    // Destroying a backend unsubscribes it and closes (flushes) the log file.
    // The frontend must outlive both.
    g_file.reset();
    g_console.reset();
    g_frontend.reset();
}

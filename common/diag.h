#pragma once

// Diagnostic instrumentation for hunting a thread that burns CPU: a per-loop
// heartbeat (iteration rate plus the CPU time the calling thread spent).
// Linux port of showfavicon-win's src/diag.h (GetThreadTimes/GetTickCount64
// are replaced by clock_gettime()). All of it compiles out unless LOG_TIMING is
// defined, so the shipped binary carries none of it; define LOG_TIMING while
// investigating and nothing at the call sites has to change.
//
// Two questions have to be answerable from the log alone: which thread is busy,
// and whether it is spinning (turning its loop as fast as it can) or doing real
// work in one place. Every long-running loop calls Heartbeat::tick() once per
// iteration; the heartbeat turns that into one line carrying the iteration count
// and the CPU time the thread spent in the window. A spin shows up as an
// absurd iteration rate and CPU that grows with wall time; an idle loop shows a
// handful of iterations and ~0 ms.

#ifdef LOG_TIMING

#include <CpputilsDebug.h>
#include <format.h>

#include <time.h>

namespace Tools {

// CPU time (user + system) consumed by the calling thread so far, in ms.
// Returns -1 when the query fails.
inline int threadCpuMs() {
    struct timespec ts {};
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) != 0) {
        return -1;
    }
    return static_cast<int>(ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL);
}

inline long long monotonicMs() {
    struct timespec ts {};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

// One line naming the calling thread and the CPU it has used in total.
inline void logThreadCpu(const char* name) {
    CPPDEBUG( format( "diag: %s thread cpu %d ms", name, threadCpuMs() ) );
}

// Per-loop heartbeat. Construct once per thread, call tick() once per iteration,
// and a line is written every `intervalSec`.
class Heartbeat {
public:
    explicit Heartbeat(const char* name, unsigned intervalSec = 60)
        : m_name(name),
          m_intervalMs(static_cast<long long>(intervalSec) * 1000LL),
          m_windowStart(monotonicMs()),
          m_windowCpuMs(threadCpuMs()),
          m_iters(0) {}

    void tick() {
        ++m_iters;
        if (monotonicMs() - m_windowStart < m_intervalMs) {
            return;
        }
        report();
    }

    // Write the line now and start a fresh window. Used where a loop cannot
    // tick, or for a forced sample.
    void report() {
        const long long now = monotonicMs();
        const int cpu = threadCpuMs();
        CPPDEBUG( format(
            "diag: %s did %d iteration(s) in %d ms, used %d ms cpu", m_name,
            static_cast<int>(m_iters),
            static_cast<int>(now - m_windowStart), cpu - m_windowCpuMs ) );
        m_iters = 0;
        m_windowStart = now;
        m_windowCpuMs = cpu;
    }

private:
    const char* m_name;
    long long m_intervalMs;
    long long m_windowStart;
    int m_windowCpuMs;
    unsigned long long m_iters;
};

}  // namespace Tools

#else  // !LOG_TIMING

namespace Tools {

// No-op stand-ins: the call sites stay untouched and cost nothing.
inline void logThreadCpu(const char*) {}

class Heartbeat {
public:
    explicit Heartbeat(const char*, unsigned = 60) {}
    void tick() {}
    void report() {}
};

}  // namespace Tools

#endif  // LOG_TIMING

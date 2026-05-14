#pragma once
#include "RingBuffer.h"
#include <thread>
#include <atomic>
#include <string>

namespace StormLog {

class DrainThread {
public:
    DrainThread(RingBuffer* rb, const char* outPath,
                int flushIntervalMs, int flushEveryNRows);
    ~DrainThread();

    void Start();
    // Caller MUST stop producers (no more Push calls) BEFORE invoking Stop().
    // Any Push that races against Stop's final drain pass may be silently dropped.
    void Stop();

private:
    void Loop();

    RingBuffer*       _rb;
    std::string       _outPath;
    int               _flushIntervalMs;
    int               _flushEveryNRows;
    std::thread       _thread;
    std::atomic<bool> _stop;
};

} // namespace StormLog

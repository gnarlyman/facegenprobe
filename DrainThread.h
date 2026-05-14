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

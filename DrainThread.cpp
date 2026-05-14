#include "DrainThread.h"
#include "CsvWriter.h"
#include <cstdio>
#include <chrono>

namespace StormLog {

DrainThread::DrainThread(RingBuffer* rb, const char* outPath, int flushIntervalMs, int flushEveryNRows)
    : _rb(rb), _outPath(outPath), _flushIntervalMs(flushIntervalMs),
      _flushEveryNRows(flushEveryNRows), _stop(false) {}

DrainThread::~DrainThread() { Stop(); }

void DrainThread::Start() {
    _stop = false;
    _thread = std::thread(&DrainThread::Loop, this);
}

void DrainThread::Stop() {
    if (!_thread.joinable()) return;
    _stop = true;
    _thread.join();
}

void DrainThread::Loop() {
    FILE* fp = nullptr;
    int rowsSinceFlush = 0;
    auto lastFlush = std::chrono::steady_clock::now();
    const int sleepMs = _flushIntervalMs / 4 > 0 ? _flushIntervalMs / 4 : 1;
    char buf[16 * 1024];

    while (true) {
        size_t n = _rb->SwapAndCopy(buf, sizeof(buf));
        if (n > 0) {
            if (!fp) {
                fp = std::fopen(_outPath.c_str(), "wb");
                if (fp) std::fputs(CsvWriter::Header(), fp);
            }
            if (fp) {
                std::fwrite(buf, 1, n, fp);
                // Count newlines for row counting.
                for (size_t i = 0; i < n; ++i) if (buf[i] == '\n') ++rowsSinceFlush;
            }
        }
        auto now = std::chrono::steady_clock::now();
        auto sinceFlush = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFlush).count();
        if (fp && (rowsSinceFlush >= _flushEveryNRows || sinceFlush >= _flushIntervalMs)) {
            std::fflush(fp);
            rowsSinceFlush = 0;
            lastFlush = now;
        }
        if (_stop) {
            // Final drain pass
            n = _rb->SwapAndCopy(buf, sizeof(buf));
            if (n > 0 && fp) std::fwrite(buf, 1, n, fp);
            if (fp) { std::fflush(fp); std::fclose(fp); }
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
    }
}

} // namespace StormLog

#include "RingBuffer.h"
#include <cstdlib>
#include <cstring>

namespace StormLog {

RingBuffer::RingBuffer(size_t bufferSize)
    : _activeLen(0), _shadowLen(0), _cap(bufferSize), _dropped(0)
{
    _active = (char*)std::malloc(bufferSize);
    _shadow = (char*)std::malloc(bufferSize);
}

RingBuffer::~RingBuffer() {
    std::free(_active);
    std::free(_shadow);
}

bool RingBuffer::Push(const void* data, size_t n) {
    std::lock_guard<std::mutex> g(_mu);
    if (_activeLen + n > _cap) {
        ++_dropped;
        return false;
    }
    std::memcpy(_active + _activeLen, data, n);
    _activeLen += n;
    return true;
}

size_t RingBuffer::SwapAndCopy(char* out, size_t outCap) {
    size_t take;
    {
        std::lock_guard<std::mutex> g(_mu);
        if (_activeLen == 0) return 0;
        // Swap
        std::swap(_active, _shadow);
        std::swap(_activeLen, _shadowLen);
        // _activeLen now reset (was _shadowLen which was 0 from prior swap or init).
        // _shadowLen now has the previous _activeLen.
        take = _shadowLen;
    }
    if (take > outCap) {
        // Caller can't take this batch; treat as drop. Reset shadow.
        _dropped += 1;
        std::lock_guard<std::mutex> g(_mu);
        _shadowLen = 0;
        return 0;
    }
    std::memcpy(out, _shadow, take);
    _shadowLen = 0;
    return take;
}

} // namespace StormLog

#pragma once
#include <cstdint>
#include <cstddef>
#include <mutex>

namespace StormLog {

// Mutex-protected double-buffered ring. Producers Push under mutex; drain
// thread SwapAndCopy briefly takes the mutex, swaps active <-> shadow,
// then copies the shadow out without holding the lock.
class RingBuffer {
public:
    explicit RingBuffer(size_t bufferSize);
    ~RingBuffer();

    // Append data. Returns false if the active buffer is full (data dropped,
    // DroppedCount incremented).
    bool Push(const void* data, size_t n);

    // Atomically swaps active and shadow, then copies the (previously active)
    // shadow into out. Returns number of bytes copied, 0 if buffer was empty.
    // If out is too small, returns 0 and discards (treated as drop).
    size_t SwapAndCopy(char* out, size_t outCap);

    uint64_t DroppedCount() const { return _dropped; }

private:
    char*       _active;
    char*       _shadow;
    size_t      _activeLen;
    size_t      _shadowLen;
    size_t      _cap;
    uint64_t    _dropped;
    std::mutex  _mu;
};

} // namespace StormLog

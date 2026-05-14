#pragma once
#include <cstdint>
#include <unordered_map>

namespace StormLog {

typedef void (*PrintFn)(const char* s);

class ConsoleSurface {
public:
    ConsoleSurface(PrintFn fn, uint64_t cooldownMs)
        : _fn(fn), _cooldownMs(cooldownMs) {}

    // Returns true if a line was printed.
    bool MaybePrintStorm(uint32_t npc_formid,
                         const char* edid,
                         const char* mod_source,
                         uint32_t cell_formid,
                         uint32_t calls_in_window,
                         uint32_t total_retries,
                         uint64_t now_ms);

private:
    PrintFn    _fn;
    uint64_t   _cooldownMs;
    std::unordered_map<uint32_t, uint64_t> _lastPrintMs;
};

} // namespace StormLog

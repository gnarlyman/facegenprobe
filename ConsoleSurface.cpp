#include "ConsoleSurface.h"
#include <cstdio>

namespace StormLog {

bool ConsoleSurface::MaybePrintStorm(uint32_t npc_formid,
                                     const char* edid,
                                     const char* mod_source,
                                     uint32_t cell_formid,
                                     uint32_t calls_in_window,
                                     uint32_t total_retries,
                                     uint64_t now_ms)
{
    auto it = _lastPrintMs.find(npc_formid);
    if (it != _lastPrintMs.end() && (now_ms - it->second) < _cooldownMs) {
        return false;
    }
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "[StormLog] STORM npc=%08X (%s / %s) calls=%u/1s totalRetries=%u cell=%08X",
        npc_formid,
        edid       ? edid       : "",
        mod_source ? mod_source : "",
        calls_in_window,
        total_retries,
        cell_formid);
    _fn(buf);
    _lastPrintMs[npc_formid] = now_ms;
    return true;
}

} // namespace StormLog

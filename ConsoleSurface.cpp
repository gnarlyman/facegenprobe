#include "ConsoleSurface.h"
#include "FormIdResolver.h"
#include "RefMap.h"
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

    // Player position is folded into the console line so ConScribe captures
    // it to disk (readable while the game runs). Caller already applied the
    // per-NPC cooldown, so this is not verbose.
    float px = 0.f, py = 0.f, pz = 0.f;
    uint32_t pcell = 0;
    bool havePos = FormIdResolver::PlayerWorldPos(px, py, pz, pcell);

    // npc=<base TESNPC FormID> is NOT prid-able. ref=<placed ACHR FormID>
    // (resolved via FlagWatch's Update hook) IS the one to `prid` in-game.
    uint32_t ref = RefMap::GetRefForBase(npc_formid & 0x00FFFFFFu);

    char buf[416];
    if (havePos) {
        std::snprintf(buf, sizeof(buf),
            "[StormLog] STORM npc=%08X ref=%08X (%s / %s) calls=%u/1s "
            "totalRetries=%u cell=%08X player=(%.0f,%.0f,%.0f) pcell=%08X",
            npc_formid, ref, edid ? edid : "", mod_source ? mod_source : "",
            calls_in_window, total_retries, cell_formid,
            px, py, pz, pcell);
    } else {
        std::snprintf(buf, sizeof(buf),
            "[StormLog] STORM npc=%08X ref=%08X (%s / %s) calls=%u/1s "
            "totalRetries=%u cell=%08X player=(unloaded)",
            npc_formid, ref, edid ? edid : "", mod_source ? mod_source : "",
            calls_in_window, total_retries, cell_formid);
    }
    _fn(buf);
    _lastPrintMs[npc_formid] = now_ms;
    return true;
}

} // namespace StormLog

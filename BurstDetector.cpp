#include "BurstDetector.h"

namespace StormLog {

BurstResult BurstState::Observe(void* npc, void* fgp, uint64_t now_ms, uint64_t windowMs) {
    BurstResult r{};
    bool gapExpired = (now_ms - lastTimeMs) > windowMs;
    bool sameTuple  = (npc == lastNpc) && (fgp == lastFgp);

    if (sameTuple && !gapExpired) {
        ++dupes;
        r.isDupe     = true;
        r.burstEnded = false;
        lastTimeMs   = now_ms;
        return r;
    }

    // Burst ends. Seal it and start a new one.
    r.burstEnded = true;
    r.endedNpc   = lastNpc;
    r.dupesAtEnd = dupes;
    r.isDupe     = false;

    lastNpc    = npc;
    lastFgp    = fgp;
    lastTimeMs = now_ms;
    dupes      = 0;
    return r;
}

} // namespace StormLog

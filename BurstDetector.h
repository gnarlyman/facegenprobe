#pragma once
#include <cstdint>

namespace StormLog {

struct BurstResult {
    bool      isDupe;       // this call was a same-NPC same-FGP repeat within window
    bool      burstEnded;   // a prior burst (or initial empty state) was just sealed
    void*     endedNpc;     // identity of the burst that ended (NPC ptr); 0 if none
    uint32_t  dupesAtEnd;   // number of dupes accumulated in the ended burst
};

struct BurstState {
    void*    lastNpc      = nullptr;
    void*    lastFgp      = nullptr;
    uint64_t lastTimeMs   = 0;
    uint32_t dupes        = 0;

    // Called once per hook entry. windowMs = max gap that counts as "same burst".
    BurstResult Observe(void* npc, void* fgp, uint64_t now_ms, uint64_t windowMs);
};

} // namespace StormLog

#pragma once
#include <cstdint>
#include <unordered_map>
#include <mutex>

// Maps a base NPC FormID (low 24 bits) -> the most-recently-updated placed
// reference (ACHR) FormID using that base. Populated from FlagWatch's
// TESCharacter::Update hook (which has the refr), consumed by the storm
// surface so the logged ID is one you can `prid` in-game.
//
// The L1 FaceGen hook only receives the base TESNPC, so it can only log the
// base FormID. During a storm the storming actor is Update'd every frame, so
// "most recent ref for this base" reliably resolves to the storming actor.
// Caveat: if several refs share a base, last-writer-wins — fine for storms
// (one actor dominates), approximate otherwise.

namespace StormLog {
namespace RefMap {

inline std::mutex& Mtx() { static std::mutex m; return m; }
inline std::unordered_map<uint32_t, uint32_t>& Map() {
    static std::unordered_map<uint32_t, uint32_t> m; return m;
}

inline void SetRefForBase(uint32_t baseFidLow24, uint32_t refrFidFull) {
    if (!baseFidLow24 || !refrFidFull) return;
    std::lock_guard<std::mutex> g(Mtx());
    Map()[baseFidLow24] = refrFidFull;
}

// Returns the ref FormID, or 0 if unknown.
inline uint32_t GetRefForBase(uint32_t baseFidLow24) {
    std::lock_guard<std::mutex> g(Mtx());
    auto it = Map().find(baseFidLow24);
    return it == Map().end() ? 0u : it->second;
}

} // namespace RefMap
} // namespace StormLog

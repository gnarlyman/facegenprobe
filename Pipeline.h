#pragma once
#include "Config.h"
#include "NpcStats.h"
#include "RingBuffer.h"
#include "DrainThread.h"
#include "ConsoleSurface.h"
#include "CsvWriter.h"
#include "BurstDetector.h"
#include <mutex>
#include <atomic>

namespace StormLog {

class Pipeline {
public:
    static Pipeline& Instance();

    // Lifecycle (called once each from Main.cpp).
    void Init(const Config& cfg, const char* csvPath, PrintFn printFn);
    void Shutdown();

    bool IsInitialized() const { return _initialized; }

    // Hook entrypoints. All thread-safe.
    // For L1: pass NPC ptr and FGP ptr (used as opaque identity), plus the
    // NPC's *FormID* for stats (caller resolves from npc->refID before call).
    void OnL1(void* npc, void* fgp, uint32_t npcFormID);
    void OnL3a(uint32_t npcFormID);
    void OnL3b();

private:
    Pipeline() = default;
    void EmitEvent(const CsvEvent& e); // formats + pushes to ring

    Config           _cfg{};
    NpcStatsTable*   _stats = nullptr;
    std::mutex       _statsMu;
    RingBuffer*      _ring  = nullptr;
    DrainThread*     _drain = nullptr;
    ConsoleSurface*  _surface = nullptr;
    std::atomic<uint32_t> _globalL3bCount{0};
    bool             _initialized = false;

    static thread_local BurstState _l1Burst; // declared in .cpp
};

} // namespace StormLog

#include "Pipeline.h"
#include "BurstDetector.h"
#include "FormIdResolver.h"
#include "CsvWriter.h"
#include "TimeMs.h"
#include <cstring>

namespace StormLog {

thread_local BurstState Pipeline::_l1Burst = {};

Pipeline& Pipeline::Instance() {
    static Pipeline p;
    return p;
}

void Pipeline::Init(const Config& cfg, const char* csvPath, PrintFn printFn) {
    _cfg     = cfg;
    _stats   = new NpcStatsTable((size_t)cfg.iMaxTrackedNpcs);
    _ring    = new RingBuffer(16 * 1024);
    _surface = new ConsoleSurface(printFn, (uint64_t)cfg.iConsoleCooldownMs);
    if (cfg.bWriteCSV) {
        _drain = new DrainThread(_ring, csvPath, cfg.iFlushIntervalMs, cfg.iFlushEveryNEvents);
        _drain->Start();
    }
    _initialized = true;
}

void Pipeline::Shutdown() {
    if (_drain) { _drain->Stop(); delete _drain; _drain = nullptr; }
    delete _surface; _surface = nullptr;
    delete _ring;    _ring    = nullptr;
    delete _stats;   _stats   = nullptr;
    _initialized = false;
}

void Pipeline::EmitEvent(const CsvEvent& e) {
    if (!_cfg.bWriteCSV || !_ring) return;
    char row[512];
    size_t n = CsvWriter::FormatRow(e, row, sizeof(row));
    if (n == 0) return;
    _ring->Push(row, n);
}

void Pipeline::OnL1(void* npc, void* fgp, uint32_t npcFormID) {
    uint64_t now = TimeMs();
    BurstResult br = _l1Burst.Observe(npc, fgp, now, (uint64_t)_cfg.iBurstWindowMs);
    // Hot path: just bump dupe counter and return.
    if (br.isDupe) return;

    // Burst boundary or first call. Touch shared map.
    // Declare surface-call captures before the lock scope so they're accessible after.
    const char* edidCopy         = nullptr;
    const char* modCopy          = nullptr;
    uint32_t    cellFid          = 0;
    uint32_t    inWindowCopy     = 0;
    uint32_t    totalRetriesCopy = 0;
    uint32_t    formIdCopy       = npcFormID;
    bool        shouldSurface    = false;

    {
        std::lock_guard<std::mutex> g(_statsMu);
        bool inserted;
        NpcStatsEntry* st = _stats->GetOrInsert(npcFormID, now, &inserted);

        // Account: each burst seals N+1 calls (the seed + N dupes), or just 1 if no dupes.
        // The current call is the *start* of a new burst, so we account the *ended* burst's
        // stats against its NPC, not the current one.
        if (br.burstEnded && br.endedNpc != nullptr && br.dupesAtEnd > 0) {
            // Find or insert the prior NPC's row to attribute the dupes.
            // For simplicity, we attribute by current FormID if endedNpc == npc (same FormID).
            // Cross-NPC burst-end attribution requires reverse-lookup, which we skip in v0.1.
        }

        if (inserted) {
            // Resolve returns borrowed const char*; copy into st->edid/mod_source buffers.
            const char* edid = nullptr; const char* mod = nullptr;
            FormIdResolver::Resolve(npcFormID, &edid, &mod);
            strncpy_s(st->edid,       sizeof(st->edid),       edid ? edid : "", _TRUNCATE);
            strncpy_s(st->mod_source, sizeof(st->mod_source), mod  ? mod  : "", _TRUNCATE);

            // Emit FIRST_SEEN.
            CsvEvent e{};
            e.time_ms          = now;
            e.layer            = CsvLayer::L1;
            e.event_type       = CsvEventType::FIRST_SEEN;
            e.npc_formid       = npcFormID;
            e.npc_edid         = st->edid;
            e.mod_source       = st->mod_source;
            e.cell_formid      = FormIdResolver::CurrentCellFormId();
            e.total_calls_l1   = st->total_l1_calls;
            e.total_retries_l1 = st->total_l1_retries;
            e.total_l3         = st->total_l3_dispatch;
            EmitEvent(e);
        }

        // Account this call.
        ++st->total_l1_calls;
        st->total_l1_retries += br.dupesAtEnd;
        st->PushRecent(now);

        // 1000 ms = calls-per-second metric for storm-threshold compare;
        // independent of BurstDetector's iBurstWindowMs (same-tuple dedup).
        uint32_t inWindow = st->CallsInWindow(now, 1000);
        if (inWindow > st->max_burst_per_sec) st->max_burst_per_sec = inWindow;

        bool isStorm = (int)inWindow >= _cfg.iBurstThresholdPerSecond;

        CsvEvent e{};
        e.time_ms          = now;
        e.layer            = CsvLayer::L1;
        e.event_type       = isStorm ? CsvEventType::STORM : CsvEventType::BURST_END;
        e.npc_formid       = npcFormID;
        e.npc_edid         = st->edid;
        e.mod_source       = st->mod_source;
        e.cell_formid      = FormIdResolver::CurrentCellFormId();
        e.burst_count      = br.dupesAtEnd;
        e.calls_in_window  = inWindow;
        e.total_calls_l1   = st->total_l1_calls;
        e.total_retries_l1 = st->total_l1_retries;
        e.total_l3         = st->total_l3_dispatch;
        EmitEvent(e);

        // Capture values needed for surface call while still holding the lock;
        // slab strings are write-once on insert so pointer capture is safe for v0.1.
        shouldSurface    = isStorm && _cfg.bConsoleSurface && _surface;
        edidCopy         = st->edid;
        modCopy          = st->mod_source;
        cellFid          = e.cell_formid;
        inWindowCopy     = inWindow;
        totalRetriesCopy = st->total_l1_retries;
    } // _statsMu released here

    // Console_Print is an engine callback that may block; call outside the lock.
    if (shouldSurface) {
        _surface->MaybePrintStorm(formIdCopy, edidCopy, modCopy,
            cellFid, inWindowCopy, totalRetriesCopy, now);
    }
}

void Pipeline::OnL3a(uint32_t npcFormID) {
    uint64_t now = TimeMs();
    std::lock_guard<std::mutex> g(_statsMu);
    bool inserted;
    NpcStatsEntry* st = _stats->GetOrInsert(npcFormID, now, &inserted);
    ++st->total_l3_dispatch;

    CsvEvent e{};
    e.time_ms     = now;
    e.layer       = CsvLayer::L3a;
    e.event_type  = CsvEventType::L3_DISPATCH;
    e.npc_formid  = npcFormID;
    e.npc_edid    = st->edid;
    e.mod_source  = st->mod_source;
    e.cell_formid = FormIdResolver::CurrentCellFormId();
    e.total_l3    = st->total_l3_dispatch;
    EmitEvent(e);
}

void Pipeline::OnL3b() {
    _globalL3bCount.fetch_add(1, std::memory_order_relaxed);
    // No per-event row; folded into L1/L3a context if needed. Counter readable
    // by future diagnostics. (Spec §"Hook layers": L3b is global rate only.)
}

} // namespace StormLog

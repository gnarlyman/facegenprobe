#pragma once
#include <cstdint>
#include <cstddef>

namespace StormLog {

struct NpcStatsEntry {
    uint32_t formID;            // 0 = free slot
    char     edid[64];
    char     mod_source[32];
    uint64_t first_seen_ms;
    uint64_t last_seen_ms;
    uint32_t total_l1_calls;
    uint32_t total_l1_retries;
    uint32_t total_l3_dispatch;
    uint32_t max_burst_per_sec;
    uint64_t last_console_ms;
    uint64_t recent_ts[16];
    uint8_t  recent_head;       // next write index
    uint8_t  recent_count;      // 0..16

    void PushRecent(uint64_t now_ms);
    uint32_t CallsInWindow(uint64_t now_ms, uint64_t windowMs) const;
};

class NpcStatsTable {
public:
    explicit NpcStatsTable(size_t capacity);
    ~NpcStatsTable();
    NpcStatsTable(const NpcStatsTable&) = delete;
    NpcStatsTable& operator=(const NpcStatsTable&) = delete;

    // Returns entry for formID, allocating if not present. If at capacity,
    // evicts the entry with the smallest last_seen_ms. Updates last_seen_ms.
    // *outInserted is true iff a new entry was allocated.
    NpcStatsEntry* GetOrInsert(uint32_t formID, uint64_t now_ms, bool* outInserted);

    size_t Size() const { return _count; }
    size_t Capacity() const { return _capacity; }

private:
    NpcStatsEntry* _slab;       // owned; _capacity entries
    uint32_t*      _hashKeys;   // open-addressed: stores formID; 0 = free
    uint16_t*      _hashIdx;    // index into _slab when key matches
    size_t         _capacity;
    size_t         _hashSize;   // power-of-two, >= 2 * capacity
    size_t         _count;

    size_t Probe(uint32_t formID) const; // returns hash slot for key (or first free)
    uint16_t FindLruIndex() const;       // smallest last_seen_ms
    void RemoveFromHash(uint32_t formID);
};

} // namespace StormLog

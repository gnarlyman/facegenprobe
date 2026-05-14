#include "NpcStats.h"
#include <cstdlib>
#include <cstring>
#include <climits>

namespace StormLog {

static size_t NextPow2(size_t n) {
    size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

void NpcStatsEntry::PushRecent(uint64_t now_ms) {
    recent_ts[recent_head] = now_ms;
    recent_head = (recent_head + 1) % 16;
    if (recent_count < 16) ++recent_count;
}

uint32_t NpcStatsEntry::CallsInWindow(uint64_t now_ms, uint64_t windowMs) const {
    uint32_t n = 0;
    for (uint8_t i = 0; i < recent_count; ++i) {
        if (now_ms - recent_ts[i] <= windowMs) ++n;
    }
    return n;
}

NpcStatsTable::NpcStatsTable(size_t capacity)
    : _capacity(capacity), _count(0)
{
    _hashSize = NextPow2(capacity * 2);
    if (_hashSize < 8) _hashSize = 8;
    _slab     = (NpcStatsEntry*)std::calloc(_capacity, sizeof(NpcStatsEntry));
    _hashKeys = (uint32_t*)     std::calloc(_hashSize, sizeof(uint32_t));
    _hashIdx  = (uint16_t*)     std::calloc(_hashSize, sizeof(uint16_t));
}

NpcStatsTable::~NpcStatsTable() {
    std::free(_slab);
    std::free(_hashKeys);
    std::free(_hashIdx);
}

size_t NpcStatsTable::Probe(uint32_t formID) const {
    // Fibonacci hash mix
    uint32_t h = formID * 2654435761u;
    size_t mask = _hashSize - 1;
    size_t i = h & mask;
    while (_hashKeys[i] != 0 && _hashKeys[i] != formID) {
        i = (i + 1) & mask;
    }
    return i;
}

uint16_t NpcStatsTable::FindLruIndex() const {
    uint64_t best = UINT64_MAX;
    uint16_t bestIdx = 0;
    for (uint16_t i = 0; i < (uint16_t)_capacity; ++i) {
        if (_slab[i].formID != 0 && _slab[i].last_seen_ms < best) {
            best = _slab[i].last_seen_ms;
            bestIdx = i;
        }
    }
    return bestIdx;
}

void NpcStatsTable::RemoveFromHash(uint32_t formID) {
    size_t i = Probe(formID);
    if (_hashKeys[i] != formID) return;
    _hashKeys[i] = 0;
    _hashIdx[i]  = 0;
    // Re-probe forward to fix gaps from linear probing.
    size_t mask = _hashSize - 1;
    size_t j = (i + 1) & mask;
    while (_hashKeys[j] != 0) {
        uint32_t k = _hashKeys[j];
        uint16_t v = _hashIdx[j];
        _hashKeys[j] = 0;
        _hashIdx[j]  = 0;
        size_t ni = Probe(k);
        _hashKeys[ni] = k;
        _hashIdx[ni]  = v;
        j = (j + 1) & mask;
    }
}

NpcStatsEntry* NpcStatsTable::GetOrInsert(uint32_t formID, uint64_t now_ms, bool* outInserted) {
    size_t i = Probe(formID);
    if (_hashKeys[i] == formID) {
        if (outInserted) *outInserted = false;
        NpcStatsEntry* e = &_slab[_hashIdx[i]];
        e->last_seen_ms = now_ms;
        return e;
    }
    // Not present. Allocate or evict.
    uint16_t slotIdx;
    if (_count < _capacity) {
        // Find first free slot.
        slotIdx = (uint16_t)_count;
        for (uint16_t k = 0; k < (uint16_t)_capacity; ++k) {
            if (_slab[k].formID == 0) { slotIdx = k; break; }
        }
        ++_count;
    } else {
        slotIdx = FindLruIndex();
        RemoveFromHash(_slab[slotIdx].formID);
    }
    NpcStatsEntry* e = &_slab[slotIdx];
    std::memset(e, 0, sizeof(*e));
    e->formID        = formID;
    e->first_seen_ms = now_ms;
    e->last_seen_ms  = now_ms;
    size_t ni = Probe(formID);
    _hashKeys[ni] = formID;
    _hashIdx[ni]  = slotIdx;
    if (outInserted) *outInserted = true;
    return e;
}

} // namespace StormLog

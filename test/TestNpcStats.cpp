#include "TestFramework.h"
#include "../NpcStats.h"
#include <cstring>

using namespace StormLog;

TEST(npcstats_first_lookup_inserts) {
    NpcStatsTable t(/*capacity*/ 8);
    bool inserted;
    NpcStatsEntry* e = t.GetOrInsert(0x000ABCDE, /*now_ms*/ 100, &inserted);
    ASSERT_TRUE(e != nullptr);
    ASSERT_TRUE(inserted);
    ASSERT_EQ(e->formID, 0x000ABCDEu);
    ASSERT_EQ(e->first_seen_ms, 100u);
    ASSERT_EQ(e->last_seen_ms,  100u);
    ASSERT_EQ(e->total_l1_calls, 0u);
}

TEST(npcstats_second_lookup_returns_existing) {
    NpcStatsTable t(/*capacity*/ 8);
    bool inserted;
    t.GetOrInsert(0x000ABCDE, 100, &inserted);
    NpcStatsEntry* e = t.GetOrInsert(0x000ABCDE, 200, &inserted);
    ASSERT_TRUE(!inserted);
    ASSERT_EQ(e->first_seen_ms, 100u);
    // last_seen_ms updated on every lookup
    ASSERT_EQ(e->last_seen_ms,  200u);
}

TEST(npcstats_fills_to_capacity_then_evicts_lru) {
    NpcStatsTable t(/*capacity*/ 4);
    bool ins;
    NpcStatsEntry* a = t.GetOrInsert(0x1, 100, &ins);
    NpcStatsEntry* b = t.GetOrInsert(0x2, 110, &ins);
    NpcStatsEntry* c = t.GetOrInsert(0x3, 120, &ins);
    NpcStatsEntry* d = t.GetOrInsert(0x4, 130, &ins);
    ASSERT_EQ(t.Size(), 4u);

    // Touch a so its last_seen advances; b stays oldest.
    t.GetOrInsert(0x1, 200, &ins);
    // Insert a new key — should evict b (oldest last_seen).
    NpcStatsEntry* e = t.GetOrInsert(0x5, 210, &ins);
    ASSERT_TRUE(ins);
    ASSERT_EQ(t.Size(), 4u);
    // Re-looking up b should re-insert as new.
    NpcStatsEntry* b2 = t.GetOrInsert(0x2, 220, &ins);
    ASSERT_TRUE(ins);
    ASSERT_EQ(b2->first_seen_ms, 220u);
}

TEST(npcstats_push_recent_ts_ring) {
    NpcStatsTable t(8);
    bool ins;
    NpcStatsEntry* e = t.GetOrInsert(0x1, 1000, &ins);
    e->PushRecent(1100);
    e->PushRecent(1500);
    e->PushRecent(1900);
    // Within 1000ms window of "now=2000": 1100, 1500, 1900 → 3.
    ASSERT_EQ(e->CallsInWindow(2000, /*windowMs*/ 1000), 3u);
    // Within 1000ms of "now=2500": only 1500, 1900 → 2.
    ASSERT_EQ(e->CallsInWindow(2500, 1000), 2u);
}

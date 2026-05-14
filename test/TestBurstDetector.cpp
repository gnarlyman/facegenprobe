#include "TestFramework.h"
#include "../BurstDetector.h"

using namespace StormLog;

TEST(burst_first_call_is_not_a_dupe) {
    BurstState s{};
    BurstResult r = s.Observe(/*npc*/(void*)0x1, /*fgp*/(void*)0x10, /*now*/100, /*windowMs*/50);
    ASSERT_TRUE(!r.isDupe);
    ASSERT_TRUE(r.burstEnded);  // first call ends any prior burst (vacuously)
    ASSERT_EQ(r.dupesAtEnd, 0u);
}

TEST(burst_same_pair_within_window_is_dupe) {
    BurstState s{};
    s.Observe((void*)0x1, (void*)0x10, 100, 50);     // first call
    BurstResult r = s.Observe((void*)0x1, (void*)0x10, 120, 50);
    ASSERT_TRUE(r.isDupe);
    ASSERT_TRUE(!r.burstEnded);
}

TEST(burst_ends_on_different_npc) {
    BurstState s{};
    s.Observe((void*)0x1, (void*)0x10, 100, 50);
    s.Observe((void*)0x1, (void*)0x10, 110, 50);
    s.Observe((void*)0x1, (void*)0x10, 120, 50);
    BurstResult r = s.Observe((void*)0x2, (void*)0x20, 130, 50);
    ASSERT_TRUE(!r.isDupe);
    ASSERT_TRUE(r.burstEnded);
    ASSERT_EQ(r.endedNpc, (void*)0x1);
    ASSERT_EQ(r.dupesAtEnd, 2u);  // calls 2 and 3 were dupes; call 1 was the seed
}

TEST(burst_ends_on_timeout) {
    BurstState s{};
    s.Observe((void*)0x1, (void*)0x10, 100, 50);
    s.Observe((void*)0x1, (void*)0x10, 110, 50);
    // 200ms gap > 50ms window
    BurstResult r = s.Observe((void*)0x1, (void*)0x10, 310, 50);
    ASSERT_TRUE(r.burstEnded);
    ASSERT_EQ(r.endedNpc, (void*)0x1);
    ASSERT_EQ(r.dupesAtEnd, 1u);
    ASSERT_TRUE(!r.isDupe);  // this call starts a new burst
}

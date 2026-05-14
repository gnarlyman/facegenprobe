#include "TestFramework.h"
#include "../TimeMs.h"

TEST(time_ms_is_monotonic) {
    uint64_t a = StormLog::TimeMs();
    uint64_t b = StormLog::TimeMs();
    ASSERT_TRUE(b >= a);
}

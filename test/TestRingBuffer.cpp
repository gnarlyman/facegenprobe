#include "TestFramework.h"
#include "../RingBuffer.h"
#include <cstring>
#include <thread>
#include <atomic>

using namespace StormLog;

TEST(ring_push_then_drain) {
    RingBuffer rb(1024);
    const char* a = "hello\n";
    const char* b = "world\n";
    ASSERT_TRUE(rb.Push(a, 6));
    ASSERT_TRUE(rb.Push(b, 6));

    char out[64];
    size_t n = rb.SwapAndCopy(out, sizeof(out));
    ASSERT_EQ(n, 12u);
    out[n] = 0;
    ASSERT_STREQ(out, "hello\nworld\n");
    // Second drain returns nothing.
    n = rb.SwapAndCopy(out, sizeof(out));
    ASSERT_EQ(n, 0u);
}

TEST(ring_push_returns_false_when_full) {
    RingBuffer rb(8);
    char filler[8] = {'a','a','a','a','a','a','a','a'};
    ASSERT_TRUE(rb.Push(filler, 8));
    ASSERT_TRUE(!rb.Push("x", 1));
    ASSERT_EQ(rb.DroppedCount(), 1u);
}

TEST(ring_after_drain_can_push_again) {
    RingBuffer rb(8);
    ASSERT_TRUE(rb.Push("12345678", 8));
    char out[16];
    rb.SwapAndCopy(out, sizeof(out));
    ASSERT_TRUE(rb.Push("abc", 3));
}

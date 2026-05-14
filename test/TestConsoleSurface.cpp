#include "TestFramework.h"
#include "../ConsoleSurface.h"
#include <vector>
#include <string>

using namespace StormLog;

static std::vector<std::string> g_printed;
static void TestPrint(const char* s) { g_printed.emplace_back(s); }

TEST(console_first_print_emits) {
    g_printed.clear();
    ConsoleSurface cs(&TestPrint, /*cooldownMs*/ 5000);
    bool emitted = cs.MaybePrintStorm(/*npc_formid*/ 0x000ABCDE, "BanditBoss", "OOO.esp",
        /*cell*/0x12345, /*calls*/12, /*retries*/47, /*now_ms*/1000);
    ASSERT_TRUE(emitted);
    ASSERT_EQ(g_printed.size(), 1u);
    ASSERT_STREQ(g_printed[0].c_str(),
        "[StormLog] STORM npc=000ABCDE (BanditBoss / OOO.esp) calls=12/1s totalRetries=47 cell=00012345");
}

TEST(console_within_cooldown_suppressed) {
    g_printed.clear();
    ConsoleSurface cs(&TestPrint, 5000);
    cs.MaybePrintStorm(0xABCD, "X", "Y.esp", 0, 1, 1, 1000);
    bool emitted = cs.MaybePrintStorm(0xABCD, "X", "Y.esp", 0, 1, 1, 1500);
    ASSERT_TRUE(!emitted);
    ASSERT_EQ(g_printed.size(), 1u);
}

TEST(console_after_cooldown_emits_again) {
    g_printed.clear();
    ConsoleSurface cs(&TestPrint, 5000);
    cs.MaybePrintStorm(0xABCD, "X", "Y.esp", 0, 1, 1, 1000);
    bool emitted = cs.MaybePrintStorm(0xABCD, "X", "Y.esp", 0, 1, 1, 6500);
    ASSERT_TRUE(emitted);
    ASSERT_EQ(g_printed.size(), 2u);
}

TEST(console_different_npc_independent_cooldown) {
    g_printed.clear();
    ConsoleSurface cs(&TestPrint, 5000);
    cs.MaybePrintStorm(0x1, "A", "Y.esp", 0, 1, 1, 1000);
    bool emitted = cs.MaybePrintStorm(0x2, "B", "Y.esp", 0, 1, 1, 1100);
    ASSERT_TRUE(emitted);
    ASSERT_EQ(g_printed.size(), 2u);
}

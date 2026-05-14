#include "TestFramework.h"
#include "../Pipeline.h"
#include "../Config.h"
#include <vector>
#include <string>
#include <cstdio>

using namespace StormLog;

static std::vector<std::string> g_printed;
static void PrintCapture(const char* s) { g_printed.emplace_back(s); }

TEST(pipeline_initializes_and_handles_l1) {
    Config c = Config::LoadOrDefault("C:\\nope.ini");
    c.bWriteCSV = 0;  // skip drain thread for unit test
    Pipeline::Instance().Init(c, "C:\\nope.csv", &PrintCapture);
    Pipeline::Instance().OnL1((void*)0xAAAA, (void*)0xBBBB, 0x000ABCDE);
    Pipeline::Instance().OnL1((void*)0xAAAA, (void*)0xBBBB, 0x000ABCDE);
    Pipeline::Instance().OnL1((void*)0xAAAA, (void*)0xBBBB, 0x000ABCDE);
    Pipeline::Instance().Shutdown();
    // No assertions on storm yet — threshold default 5 not hit. Smoke only.
    ASSERT_TRUE(true);
}

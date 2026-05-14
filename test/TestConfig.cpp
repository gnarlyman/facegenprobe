#include <windows.h>
#include "TestFramework.h"
#include "../Config.h"
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace StormLog;

static std::string MakeTempIni(const char* contents) {
    char tmpDir[260], tmpFile[260];
    GetTempPathA(sizeof(tmpDir), tmpDir);
    GetTempFileNameA(tmpDir, "slc", 0, tmpFile);
    FILE* f = std::fopen(tmpFile, "w");
    std::fputs(contents, f);
    std::fclose(f);
    return tmpFile;
}

TEST(config_defaults_when_file_missing) {
    Config c = Config::LoadOrDefault("C:\\does\\not\\exist.ini");
    ASSERT_EQ(c.bWriteCSV, 1);
    ASSERT_EQ(c.bConsoleSurface, 1);
    ASSERT_EQ(c.iFlushIntervalMs, 1000);
    ASSERT_EQ(c.iFlushEveryNEvents, 100);
    ASSERT_EQ(c.iBurstThresholdPerSecond, 5);
    ASSERT_EQ(c.iConsoleCooldownMs, 5000);
    ASSERT_EQ(c.iBurstWindowMs, 50);
    ASSERT_EQ(c.iMaxTrackedNpcs, 4096);
    ASSERT_EQ(c.bEnableLayer1, 1);
    ASSERT_EQ(c.bEnableLayer3a, 1);
    ASSERT_EQ(c.bEnableLayer3b, 1);
}

TEST(config_overrides_from_file) {
    std::string p = MakeTempIni(
        "[Logging]\n"
        "bWriteCSV=0\n"
        "bConsoleSurface=1\n"
        "[Detection]\n"
        "iBurstThresholdPerSecond=10\n"
        "iMaxTrackedNpcs=256\n"
        "[Hooks]\n"
        "bEnableLayer3a=0\n"
    );
    Config c = Config::LoadOrDefault(p.c_str());
    ASSERT_EQ(c.bWriteCSV, 0);
    ASSERT_EQ(c.iBurstThresholdPerSecond, 10);
    ASSERT_EQ(c.iMaxTrackedNpcs, 256);
    ASSERT_EQ(c.bEnableLayer3a, 0);
    // Unspecified keys keep defaults.
    ASSERT_EQ(c.iFlushIntervalMs, 1000);
    ASSERT_EQ(c.bEnableLayer3b, 1);
    std::remove(p.c_str());
}

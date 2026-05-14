#include "Config.h"
#include <windows.h>

namespace StormLog {

Config Config::LoadOrDefault(const char* iniPath) {
    Config c{};
    // Defaults
    c.bWriteCSV                = 1;
    c.bConsoleSurface          = 1;
    c.iFlushIntervalMs         = 1000;
    c.iFlushEveryNEvents       = 100;
    c.iBurstThresholdPerSecond = 5;
    c.iConsoleCooldownMs       = 5000;
    c.iBurstWindowMs           = 50;
    c.iMaxTrackedNpcs          = 4096;
    c.bEnableLayer1            = 1;
    c.bEnableLayer3a           = 1;
    c.bEnableLayer3b           = 1;
    c.bEnableFlagWatch         = 0;

    if (GetFileAttributesA(iniPath) == INVALID_FILE_ATTRIBUTES) return c;

    c.bWriteCSV                = GetPrivateProfileIntA("Logging",   "bWriteCSV",                c.bWriteCSV,                iniPath);
    c.bConsoleSurface          = GetPrivateProfileIntA("Logging",   "bConsoleSurface",          c.bConsoleSurface,          iniPath);
    c.iFlushIntervalMs         = GetPrivateProfileIntA("Logging",   "iFlushIntervalMs",         c.iFlushIntervalMs,         iniPath);
    c.iFlushEveryNEvents       = GetPrivateProfileIntA("Logging",   "iFlushEveryNEvents",       c.iFlushEveryNEvents,       iniPath);
    c.iBurstThresholdPerSecond = GetPrivateProfileIntA("Detection", "iBurstThresholdPerSecond", c.iBurstThresholdPerSecond, iniPath);
    c.iConsoleCooldownMs       = GetPrivateProfileIntA("Detection", "iConsoleCooldownMs",       c.iConsoleCooldownMs,       iniPath);
    c.iBurstWindowMs           = GetPrivateProfileIntA("Detection", "iBurstWindowMs",           c.iBurstWindowMs,           iniPath);
    c.iMaxTrackedNpcs          = GetPrivateProfileIntA("Detection", "iMaxTrackedNpcs",          c.iMaxTrackedNpcs,          iniPath);
    c.bEnableLayer1            = GetPrivateProfileIntA("Hooks",     "bEnableLayer1",            c.bEnableLayer1,            iniPath);
    c.bEnableLayer3a           = GetPrivateProfileIntA("Hooks",     "bEnableLayer3a",           c.bEnableLayer3a,           iniPath);
    c.bEnableLayer3b           = GetPrivateProfileIntA("Hooks",     "bEnableLayer3b",           c.bEnableLayer3b,           iniPath);
    c.bEnableFlagWatch         = GetPrivateProfileIntA("Hooks",     "bEnableFlagWatch",         c.bEnableFlagWatch,         iniPath);

    return c;
}

} // namespace StormLog

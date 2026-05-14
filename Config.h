#pragma once

namespace StormLog {

struct Config {
    // [Logging]
    int bWriteCSV;
    int bConsoleSurface;
    int iFlushIntervalMs;
    int iFlushEveryNEvents;
    // [Detection]
    int iBurstThresholdPerSecond;
    int iConsoleCooldownMs;
    int iBurstWindowMs;
    int iMaxTrackedNpcs;
    // [Hooks]
    int bEnableLayer1;
    int bEnableLayer3a;
    int bEnableLayer3b;

    static Config LoadOrDefault(const char* iniPath);
};

} // namespace StormLog

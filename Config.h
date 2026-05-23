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
    int iBurstStormThreshold;   // STORM if a single burst's collapsed retry count >= this
    int iReportIntervalMs;      // periodic console report of all storming NPCs (0=off)
    int iConsoleCooldownMs;
    int iBurstWindowMs;
    int iMaxTrackedNpcs;
    // [Hooks]
    int bEnableLayer1;
    int bEnableLayer3a;
    int bEnableLayer3b;
    int bEnableFlagWatch;

    static Config LoadOrDefault(const char* iniPath);
};

} // namespace StormLog

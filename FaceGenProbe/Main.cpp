// Main.cpp - OBSE entry point for FaceGenProbe

#include "obse/PluginAPI.h"
#include "Detours/detours.h"

IDebugLog gLog("FaceGenProbe.log");
PluginHandle g_pluginHandle = kPluginHandle_Invalid;

// Forward declaration
bool InstallFaceGenHooks();

extern "C" {

bool OBSEPlugin_Query(const OBSEInterface* obse, PluginInfo* info)
{
    info->infoVersion = PluginInfo::kInfoVersion;
    info->name        = "FaceGenProbe";
    info->version     = 1;

    if (obse->obseVersion < 21) return false;
    if (obse->isEditor)         return false;

    return true;
}

bool OBSEPlugin_Load(const OBSEInterface* obse)
{
    g_pluginHandle = obse->GetPluginHandle();

    _MESSAGE("FaceGenProbe loading...");

    if (!InstallFaceGenHooks()) {
        _ERROR("Failed to install FaceGen hooks.");
        return false;
    }

    _MESSAGE("FaceGenProbe loaded successfully.");
    return true;
}

BOOL WINAPI DllMain(HANDLE, DWORD, LPVOID) { return TRUE; }

} // extern "C"
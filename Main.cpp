#include "obse/PluginAPI.h"
#include "obse/GameAPI.h"
#include "VersionInfo.h"
#include "Config.h"
#include "Pipeline.h"
#include "HookL1.h"
#include "HookL3.h"
#include <windows.h>
#include <string>
#include <ctime>

// Console_Print is declared extern in GameAPI.h but its definition lives in
// GameAPI.cpp which pulls in too many symbols.  Define it here directly.
const _Console_Print Console_Print = (_Console_Print)0x00579B9B;

IDebugLog gLog("StormLog.log");

PluginHandle            g_pluginHandle = kPluginHandle_Invalid;
OBSEMessagingInterface* g_messaging    = nullptr;

static StormLog::Config g_cfg;
static std::string      g_csvPath;

static void ConsolePrintAdapter(const char* s) {
    Console_Print("%s", s);
}

static std::string MakeSessionCsvPath() {
    std::time_t t = std::time(nullptr);
    std::tm tm; localtime_s(&tm, &t);
    char fname[64];
    std::strftime(fname, sizeof(fname), "%Y-%m-%d_%H%M%S.csv", &tm);

    // Ensure directory exists.
    CreateDirectoryA("Data\\OBSE\\Plugins\\StormLog", NULL);
    std::string p = "Data\\OBSE\\Plugins\\StormLog\\";
    p += fname;
    return p;
}

static void OnPostPostLoad() {
    _MESSAGE("StormLog: PostPostLoad — loading config and installing hooks.");
    g_cfg     = StormLog::Config::LoadOrDefault("Data\\OBSE\\Plugins\\StormLog.ini");
    g_csvPath = MakeSessionCsvPath();
    StormLog::Pipeline::Instance().Init(g_cfg, g_csvPath.c_str(), &ConsolePrintAdapter);

    if (g_cfg.bEnableLayer1) {
        if (!StormLog::HookL1::Install()) _ERROR("HookL1 install failed");
        else                              _MESSAGE("HookL1 installed.");
    }
    if (g_cfg.bEnableLayer3a || g_cfg.bEnableLayer3b) {
        if (!StormLog::HookL3::Install(g_cfg.bEnableLayer3a != 0, g_cfg.bEnableLayer3b != 0))
            _ERROR("HookL3 install failed");
        else
            _MESSAGE("HookL3 installed (3a=%d 3b=%d).", g_cfg.bEnableLayer3a, g_cfg.bEnableLayer3b);
    }
}

static void OnExitGame() {
    _MESSAGE("StormLog: ExitGame — shutting down pipeline.");
    StormLog::Pipeline::Instance().Shutdown();
}

static void MessageHandler(OBSEMessagingInterface::Message* msg) {
    switch (msg->type) {
        case OBSEMessagingInterface::kMessage_PostPostLoad:   OnPostPostLoad(); break;
        case OBSEMessagingInterface::kMessage_ExitGame:       OnExitGame();     break;
        case OBSEMessagingInterface::kMessage_ExitToMainMenu: OnExitGame();     break;
    }
}

extern "C" {

bool OBSEPlugin_Query(const OBSEInterface* obse, PluginInfo* info)
{
    _MESSAGE("StormLog %d.%d.%d.%d initializing...",
        VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION, VERSION_BUILD);

    info->infoVersion = PluginInfo::kInfoVersion;
    info->name        = "StormLog";
    info->version     = PACKED_SME_VERSION;

    g_pluginHandle = obse->GetPluginHandle();
    if (obse->obseVersion < 21) {
        _ERROR("OBSE too old (got %08X, need >= 21)", obse->obseVersion);
        return false;
    }
    if (obse->isEditor) {
        _MESSAGE("StormLog skips editor mode.");
        return false;
    }
    if (obse->oblivionVersion != OBLIVION_VERSION) {
        _MESSAGE("Unsupported runtime version %08X", obse->oblivionVersion);
        return false;
    }

    g_messaging = (OBSEMessagingInterface*)obse->QueryInterface(kInterface_Messaging);
    if (!g_messaging) {
        _ERROR("Messaging interface not found");
        return false;
    }

    return true;
}

bool OBSEPlugin_Load(const OBSEInterface* /*obse*/)
{
    g_messaging->RegisterListener(g_pluginHandle, "OBSE", MessageHandler);
    _MESSAGE("StormLog loaded; PostPostLoad will install hooks.");
    return true;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) { return TRUE; }

} // extern "C"

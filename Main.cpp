#include "obse/PluginAPI.h"
#include "obse/GameAPI.h"
#include "VersionInfo.h"

IDebugLog gLog("StormLog.log");

PluginHandle            g_pluginHandle = kPluginHandle_Invalid;
OBSEMessagingInterface* g_messaging    = nullptr;

static void MessageHandler(OBSEMessagingInterface::Message* msg)
{
    if (msg->type == OBSEMessagingInterface::kMessage_PostPostLoad) {
        _MESSAGE("StormLog: PostPostLoad reached, hooks will install here in later tasks.");
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

bool OBSEPlugin_Load(const OBSEInterface* obse)
{
    g_messaging->RegisterListener(g_pluginHandle, "OBSE", MessageHandler);
    _MESSAGE("StormLog loaded; registered for OBSE messages.");
    return true;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) { return TRUE; }

} // extern "C"

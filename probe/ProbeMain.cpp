#include "obse/PluginAPI.h"
#include "obse/GameAPI.h"
#include <windows.h>
#include "../Detours/detours.h"
#pragma comment(lib, "../Detours/detours.lib")

IDebugLog gLog("StormLogProbe.log");

PluginHandle            g_h = kPluginHandle_Invalid;
OBSEMessagingInterface* g_m = nullptr;

typedef void (__thiscall *Fn)(void*);
static Fn s_orig_L3a = (Fn)0x004353D0;
static Fn s_orig_L3b = (Fn)0x00430DE0;

static LONG volatile s_l3a_count = 0;
static LONG volatile s_l3b_count = 0;

static void __fastcall HookL3a(void* self, void*) {
    LONG n = InterlockedIncrement(&s_l3a_count);
    if ((n & 0xFF) == 1) _MESSAGE("PROBE L3a fire #%ld self=%p", n, self);
    s_orig_L3a(self);
}
static void __fastcall HookL3b(void* self, void*) {
    LONG n = InterlockedIncrement(&s_l3b_count);
    if ((n & 0xFF) == 1) _MESSAGE("PROBE L3b fire #%ld self=%p", n, self);
    s_orig_L3b(self);
}

static void OnPostPostLoad() {
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)s_orig_L3a, (PVOID)HookL3a);
    DetourAttach(&(PVOID&)s_orig_L3b, (PVOID)HookL3b);
    LONG r = DetourTransactionCommit();
    _MESSAGE("Probe install result: %ld", r);
}

static void Handler(OBSEMessagingInterface::Message* m) {
    if (m->type == OBSEMessagingInterface::kMessage_PostPostLoad) OnPostPostLoad();
}

extern "C" {
bool OBSEPlugin_Query(const OBSEInterface* obse, PluginInfo* info) {
    info->infoVersion = PluginInfo::kInfoVersion;
    info->name        = "StormLogProbe";
    info->version     = 1;
    g_h = obse->GetPluginHandle();
    if (obse->obseVersion < 21) return false;
    if (obse->isEditor)         return false;
    g_m = (OBSEMessagingInterface*)obse->QueryInterface(kInterface_Messaging);
    return g_m != nullptr;
}
bool OBSEPlugin_Load(const OBSEInterface*) {
    g_m->RegisterListener(g_h, "OBSE", Handler);
    return true;
}
BOOL WINAPI DllMain(HANDLE, DWORD, LPVOID) { return TRUE; }
}

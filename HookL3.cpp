#include "HookL3.h"
#include "Pipeline.h"
#include "obse/GameObjects.h"
#include <windows.h>
#include "Detours/detours.h"

#pragma comment(lib, "Detours/detours.lib")

namespace StormLog {
namespace HookL3 {

static const UInt32 kAddr_QueuedHead_Run        = 0x004353D0;
static const UInt32 kAddr_BSTaskThread_Runnable = 0x00430DE0;

// QueuedHead struct layout: spec marks the NPC pointer offset as an open
// implementation question. Probe DLL (Task 16) determines this; pre-bake 0
// for v0.1 and treat L3a attribution as "unknown FormID" until verified.
static const ptrdiff_t kQueuedHead_NpcOffset = 0; // TODO: probe-determined

typedef void (__thiscall *Fn_QueuedHead_Run)(void* self);
typedef void (__thiscall *Fn_BSTaskThread_Runnable)(void* self);

static Fn_QueuedHead_Run         s_origL3a = nullptr;
static Fn_BSTaskThread_Runnable  s_origL3b = nullptr;

static void __fastcall HookL3a(void* self, void* /*edx*/) {
    uint32_t npcFormID = 0;
    if (kQueuedHead_NpcOffset != 0) {
        TESNPC** npcPtr = (TESNPC**)((char*)self + kQueuedHead_NpcOffset);
        if (*npcPtr) npcFormID = (*npcPtr)->refID;
    }
    Pipeline::Instance().OnL3a(npcFormID);
    s_origL3a(self);
}

static void __fastcall HookL3b(void* self, void* /*edx*/) {
    Pipeline::Instance().OnL3b();
    s_origL3b(self);
}

bool Install(bool enable3a, bool enable3b) {
    LONG err = NO_ERROR;
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    if (enable3a) {
        s_origL3a = (Fn_QueuedHead_Run)kAddr_QueuedHead_Run;
        err |= DetourAttach(&(PVOID&)s_origL3a, (PVOID)HookL3a);
    }
    if (enable3b) {
        s_origL3b = (Fn_BSTaskThread_Runnable)kAddr_BSTaskThread_Runnable;
        err |= DetourAttach(&(PVOID&)s_origL3b, (PVOID)HookL3b);
    }
    DetourTransactionCommit();
    return err == NO_ERROR;
}

}} // namespace StormLog::HookL3

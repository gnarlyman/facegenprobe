#include "HookL1.h"
#include "Pipeline.h"
#include "obse/GameForms.h"
#include "obse/GameObjects.h"
#include "Detours/detours.h"

#pragma comment(lib, "Detours/detours.lib")

namespace StormLog {
namespace HookL1 {

// Game address from Blockhead InstanceAbstraction:
//   kTESRace_GetFaceGenHeadParameters = { 0x0052CD50, 0x004E6AA0 }
//   Game field (EditorMode == false) = 0x0052CD50
// Plan comment listed 0x004F0790 — that is wrong; Blockhead's verified value is used here.
static const UInt32 kAddr_TESRace_GetFaceGenHeadParameters = 0x0052CD50;

typedef void (__thiscall *Fn_GetFaceGenHeadParameters)(TESRace* race, TESNPC* npc, void* fgp);
static Fn_GetFaceGenHeadParameters s_orig = nullptr;

static void __fastcall HookFn(TESRace* race, void* /*edx*/, TESNPC* npc, void* fgp) {
    if (npc && fgp) {
        Pipeline::Instance().OnL1(npc, fgp, npc->refID);
    }
    s_orig(race, npc, fgp);
}

bool Install() {
    s_orig = reinterpret_cast<Fn_GetFaceGenHeadParameters>(kAddr_TESRace_GetFaceGenHeadParameters);
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(reinterpret_cast<PVOID*>(&s_orig), HookFn);
    LONG result = DetourTransactionCommit();
    return result == NO_ERROR;
}

}} // namespace StormLog::HookL1

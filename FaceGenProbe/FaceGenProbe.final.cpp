// FaceGenProbe.cpp
// Fix: Mounted NPCs with FaceGen race data cause infinite FaceGen task re-scheduling.
// Root cause: TESNPC_FaceGenFiller reads thisPtr+0xE8 (FaceGen race data) directly;
//   for mounted NPCs it takes the SilentSkipLoop path which fails for creature mounts,
//   causing re-scheduling at ~130/sec.
// Fix: Zero thisPtr+0xE8 before calling the original Filler for mounted NPCs that have
//   race data, forcing the DefaultGetter+FallbackPopulator path which completes.
//   Restore the original value afterward.
// Note: The vtable+0xF4 check on thisPtr+0x1A0 does NOT work — that offset points to
//   a different object than the scheduling mount at NPC+0x20 used in 43b990.

#include "Detours/detours.h"
#include <cstdio>
#include <mutex>
#include <windows.h>

#pragma comment(lib, "D:\\Modlists\\_clones\\StormLog\\Detours\\detours.lib")

namespace RTLog {
    inline std::mutex& Mtx() { static std::mutex m; return m; }
    inline FILE*&      Fp()  { static FILE* f = nullptr; return f; }

    inline void Write(const char* fmt, ...) {
        std::lock_guard<std::mutex> lk(Mtx());
        if (!Fp()) return;

        time_t t = time(nullptr);
        struct tm tm; localtime_s(&tm, &t);
        fprintf(Fp(), "[%02d:%02d:%02d] ", tm.tm_hour, tm.tm_min, tm.tm_sec);

        va_list a; va_start(a, fmt);
        vfprintf(Fp(), fmt, a);
        va_end(a);

        fprintf(Fp(), "\n");
        fflush(Fp());
    }

    inline void Init() {
        std::lock_guard<std::mutex> lk(Mtx());
        if (Fp()) return;

        HMODULE hm = nullptr;
        GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&Init), &hm);

        char dllPath[MAX_PATH] = {};
        if (!GetModuleFileNameA(hm, dllPath, MAX_PATH)) return;

        char* sep = nullptr;
        for (char* p = dllPath; *p; ++p) if (*p == '\\' || *p == '/') sep = p;
        if (!sep) return;
        *sep = '\0';

        char full[MAX_PATH];
        if (snprintf(full, sizeof(full), "%s\\facegenprobe_realtime.log", dllPath) <= 0) return;

        Fp() = _fsopen(full, "w", _SH_DENYWR);
        if (Fp()) {
            fprintf(Fp(), "=== FaceGenProbe real-time log opened (%s) ===\n", full);
            fflush(Fp());
        }
    }
}

static const UInt32 kAddr_TESNPC_FaceGenFiller = 0x005221c0;

// thisPtr+0xE8 = FaceGen race data pointer (in_ECX[0x3a]; 0x3a*4=0xE8)
// thisPtr+0x1A0 = mount pointer
// thisPtr+0xC = FormID
// Note: thisPtr+0x1A0 is NOT the same object as the scheduling mount at NPC+0x20.
// The vtable+0xF4 FaceGen check works on NPC+0x20 (param_5 in 43b990), not 0x1A0.

typedef void (__thiscall *Fn_FaceGenFiller)(void* thisPtr, void* param1);
static Fn_FaceGenFiller s_origFiller = nullptr;

static void __fastcall Hooked_TESNPC_FaceGenFiller(void* thisPtr, void* /*edx*/, void* param1)
{
    if (!thisPtr) {
        s_origFiller(thisPtr, param1);
        return;
    }

    UInt32 formID = *reinterpret_cast<UInt32*>((char*)thisPtr + 0xC);

    if ((formID & 0xFF000000) == 0) {
        void* mount = *(void**)((char*)thisPtr + 0x1A0);
        if (mount) {
            UInt32 raceData = *reinterpret_cast<UInt32*>((char*)thisPtr + 0xE8);
            if (raceData != 0) {
                *reinterpret_cast<UInt32*>((char*)thisPtr + 0xE8) = 0;
                s_origFiller(thisPtr, param1);
                *reinterpret_cast<UInt32*>((char*)thisPtr + 0xE8) = raceData;
                return;
            }
        }
    }

    s_origFiller(thisPtr, param1);
}

bool InstallFaceGenHooks()
{
    RTLog::Init();

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    s_origFiller = (Fn_FaceGenFiller)kAddr_TESNPC_FaceGenFiller;
    DetourAttach(&(PVOID&)s_origFiller, (PVOID)Hooked_TESNPC_FaceGenFiller);

    LONG result = DetourTransactionCommit();
    if (result != NO_ERROR) {
        _ERROR("DetourTransactionCommit failed: %d", result);
        return false;
    }

    _MESSAGE("FaceGenProbe initialized");
    _MESSAGE("  TESNPC_FaceGenFiller @ %08X (mounted race-data zeroing fix)", kAddr_TESNPC_FaceGenFiller);
    RTLog::Write("[INIT] FaceGenProbe hooks installed (Filler mounted race-data zeroing)");

    return true;
}
// FaceGenProbe.cpp
// Build 31: Baseline + retAddr dedup logging + 43cae0 re-queue suppression.
// Identifies ALL unique return addresses per mounted formID without new hooks.

#include "Detours/detours.h"
#include <cstdio>
#include <intrin.h>
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
static const UInt32 kAddr_43b990             = 0x0043b990;
static const UInt32 kAddr_43cae0             = 0x0043cae0;

// Oblivion.exe image base (needed to compute RVA from return addresses)
static const UInt32 kOblivionBase = 0x00400000;

typedef void (__thiscall *Fn_FaceGenFiller)(void* thisPtr, void* param1);
static Fn_FaceGenFiller s_origFiller = nullptr;

typedef UInt32 (__stdcall *Fn_43b990)(UInt32 param_1, UInt32 param_2, UInt32 param_3, UInt32 param_4, UInt32 mount);
static Fn_43b990 s_orig43b990 = nullptr;

typedef void (__thiscall *Fn_43cae0)(void* thisPtr);
static Fn_43cae0 s_orig43cae0 = nullptr;
static LONG s_suppressedRequeues = 0;

struct FormCounter { UInt32 formID; LONG count; };
static const int kMaxCounters = 128;
static FormCounter s_counters[kMaxCounters];
static int s_counterCount = 0;
static CRITICAL_SECTION s_counterCS;
static LONG s_mountedFillerCount = 0;
static const LONG s_dumpInterval = 50;

static LONG CountFiller(UInt32 formID) {
    LONG result = 1;
    EnterCriticalSection(&s_counterCS);
    for (int i = 0; i < s_counterCount; i++) {
        if (s_counters[i].formID == formID) {
            result = InterlockedIncrement(&s_counters[i].count) + 1;
            LeaveCriticalSection(&s_counterCS);
            return result;
        }
    }
    if (s_counterCount < kMaxCounters) {
        s_counters[s_counterCount].formID = formID;
        s_counters[s_counterCount].count = 1;
        s_counterCount++;
    }
    LeaveCriticalSection(&s_counterCS);
    return result;
}

// Dedup return addresses per formID to avoid log spam
static UInt32 s_lastRetAddr[64] = {};
static UInt32 s_lastFormID[64] = {};
static int s_dedupIdx = 0;

static bool LogDedup(UInt32 formID, UInt32 retAddr) {
    for (int i = 0; i < s_dedupIdx; i++) {
        if (s_lastFormID[i] == formID && s_lastRetAddr[i] == retAddr) return true;
    }
    if (s_dedupIdx < 64) {
        s_lastFormID[s_dedupIdx] = formID;
        s_lastRetAddr[s_dedupIdx] = retAddr;
        s_dedupIdx++;
    }
    return false;
}

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
            LONG count = CountFiller(formID);
            UInt32 retAddr = (UInt32)_ReturnAddress();

            // Log each unique (formID, retAddr) once
            if (!LogDedup(formID, retAddr)) {
                RTLog::Write("[Filler] retAddr=%08X (RVA %08X) formID=%08X mount=%08X",
                    retAddr, retAddr - kOblivionBase, formID, (UInt32)mount);
            }

            UInt32& raceData = *reinterpret_cast<UInt32*>((char*)thisPtr + 0xE8);
            UInt32 savedRaceData = raceData;
            raceData = 0;
            s_origFiller(thisPtr, param1);
            raceData = savedRaceData;

            LONG total = InterlockedIncrement(&s_mountedFillerCount);
            if (total % s_dumpInterval == 0) {
                EnterCriticalSection(&s_counterCS);
                RTLog::Write("=== FILLER CALL COUNT DUMP ===");
                for (int i = 0; i < s_counterCount; i++)
                    RTLog::Write("  formID=%08X total=%d", s_counters[i].formID, s_counters[i].count);
                RTLog::Write("  suppressed_requeues=%d", s_suppressedRequeues);
                RTLog::Write("=== END DUMP ===");
                LeaveCriticalSection(&s_counterCS);
            }
            return;
        }
    }

    s_origFiller(thisPtr, param1);
}

static UInt32 __stdcall Hooked_43b990(UInt32 param_1, UInt32 param_2, UInt32 param_3, UInt32 param_4, UInt32 mount)
{
    if (mount != 0) {
        __try {
            UInt32 facegenObj = *reinterpret_cast<UInt32*>(reinterpret_cast<char*>(mount) + 0x1C);
            if (facegenObj == 0 || facegenObj == 0xFFFFFFFF) {
                *reinterpret_cast<UInt32*>(param_1) = 0;
                return param_1;
            }
            UInt32 vtbl = *reinterpret_cast<UInt32*>(reinterpret_cast<char*>(facegenObj));
            UInt32 facegenFn = *reinterpret_cast<UInt32*>(reinterpret_cast<char*>(vtbl) + 0xF4);
            typedef char (__thiscall *Fn_FaceGenCheck)(void* thisPtr);
            char result = reinterpret_cast<Fn_FaceGenCheck>(facegenFn)(reinterpret_cast<void*>(facegenObj));
            if (result == 0) {
                *reinterpret_cast<UInt32*>(param_1) = 0;
                return param_1;
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {
        }
    }

    return s_orig43b990(param_1, param_2, param_3, param_4, mount);
}

static void __fastcall Hooked_43cae0(void* thisPtr, void* /*edx*/)
{
    char* ecx = reinterpret_cast<char*>(thisPtr);
    int obj32 = *reinterpret_cast<int*>(ecx + 0x20);
    int origObj3C = 0;
    bool patched = false;

    if (obj32 != 0) {
        __try {
            int ref28 = *reinterpret_cast<int*>(ecx + 0x28);
            if (ref28 != 0) {
                UInt32 vtbl = *reinterpret_cast<UInt32*>(reinterpret_cast<char*>(obj32));
                UInt32 isLoadedFn = *reinterpret_cast<UInt32*>(reinterpret_cast<char*>(vtbl) + 400);
                typedef char (__thiscall *Fn_IsLoaded)(void*);
                char isLoaded = reinterpret_cast<Fn_IsLoaded>(isLoadedFn)(reinterpret_cast<void*>(obj32));
                if (isLoaded == 0) {
                    int fgObj = *reinterpret_cast<int*>(reinterpret_cast<char*>(obj32) + 0x1C);
                    if (fgObj != 0) {
                        UInt32 fgVtbl = *reinterpret_cast<UInt32*>(reinterpret_cast<char*>(fgObj));
                        UInt32 fgFn = *reinterpret_cast<UInt32*>(reinterpret_cast<char*>(fgVtbl) + 0xF4);
                        typedef char (__thiscall *Fn_FGC)(void*);
                        char fgResult = reinterpret_cast<Fn_FGC>(fgFn)(reinterpret_cast<void*>(fgObj));
                        if (fgResult == 0) {
                            origObj3C = *reinterpret_cast<int*>(reinterpret_cast<char*>(obj32) + 0x3C);
                            if (origObj3C == 0) {
                                *reinterpret_cast<int*>(reinterpret_cast<char*>(obj32) + 0x3C) = fgObj;
                                patched = true;
                                InterlockedIncrement(&s_suppressedRequeues);
                            }
                        }
                    }
                }
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) { patched = false; }
    }

    s_orig43cae0(thisPtr);
    if (patched) *reinterpret_cast<int*>(reinterpret_cast<char*>(obj32) + 0x3C) = origObj3C;
}

bool InstallFaceGenHooks()
{
    RTLog::Init();
    InitializeCriticalSection(&s_counterCS);

    RTLog::Write("[INIT] InstallFaceGenHooks() entered");

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    s_origFiller = (Fn_FaceGenFiller)kAddr_TESNPC_FaceGenFiller;
    DetourAttach(&(PVOID&)s_origFiller, (PVOID)Hooked_TESNPC_FaceGenFiller);

    s_orig43b990 = (Fn_43b990)kAddr_43b990;
    DetourAttach(&(PVOID&)s_orig43b990, (PVOID)Hooked_43b990);

    s_orig43cae0 = (Fn_43cae0)kAddr_43cae0;
    DetourAttach(&(PVOID&)s_orig43cae0, (PVOID)Hooked_43cae0);

    LONG result = DetourTransactionCommit();
    if (result != NO_ERROR) {
        _ERROR("DetourTransactionCommit failed: %d", result);
        return false;
    }

    _MESSAGE("FaceGenProbe initialized");
    _MESSAGE("  TESNPC_FaceGenFiller @ %08X (E8 zero/restore + caller logging)", kAddr_TESNPC_FaceGenFiller);
    _MESSAGE("  FUN_0043b990 @ %08X (creature mount redirect)", kAddr_43b990);
    _MESSAGE("  FUN_0043cae0 @ %08X (re-queue suppression)", kAddr_43cae0);
    RTLog::Write("[INIT] FaceGenProbe Build 31 (E8 + 43b990 + 43cae0 + retAddr dedup)");

    return true;
}
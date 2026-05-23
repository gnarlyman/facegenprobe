// Build 33: NOP the Filler call inside HelperPopulator at 0x0052CD8C.
// Replaces 5-byte "call TESNPC_FaceGenFiller" with NOPs.
// HelperPopulator still copies NPC data to output buffer — no crash risk.
// Combined with existing Filler E8 hook and 43b990 hook.

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
static const UInt32 kAddr_FillerCallInHelper  = 0x0052CD8C;
static const UInt32 kAddr_FUN_0043eb80         = 0x0043EB80;

// Oblivion.exe image base (needed to compute RVA from return addresses)
static const UInt32 kOblivionBase = 0x00400000;

typedef void (__thiscall *Fn_FaceGenFiller)(void* thisPtr, void* param1);
static Fn_FaceGenFiller s_origFiller = nullptr;

typedef UInt32 (__stdcall *Fn_43b990)(UInt32 param_1, UInt32 param_2, UInt32 param_3, UInt32 param_4, UInt32 mount);
static Fn_43b990 s_orig43b990 = nullptr;

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
            UInt32 retAddr = (UInt32)_ReturnAddress();
            UInt32 retRVA = retAddr - kOblivionBase;

            if (!LogDedup(formID, retAddr)) {
                RTLog::Write("[Filler] retAddr=%08X (RVA %08X) formID=%08X mount=%08X",
                    retAddr, retRVA, formID, (UInt32)mount);
            }

            // CALLTRACE PROBE: On 6th call for 000700CC, walk EBP chain and log full calltrace
            if (formID == 0x000700CC) {
                static LONG s_700CC_count = 0;
                LONG n = InterlockedIncrement(&s_700CC_count);
                if (n == 6) {
                    RTLog::Write("=== CALLTRACE for 000700CC (call #%d) ===", n);
                    RTLog::Write("  [0] retAddr=%08X (our Filler hook)", retAddr);
                    DWORD myEsp;
                    __asm mov myEsp, esp
                    RTLog::Write("  --- stack scan from esp=%08X ---", myEsp);
                    int found = 0;
                    for (DWORD* p = (DWORD*)(myEsp & ~3); found < 12 && (DWORD)p < myEsp + 0x400; p++) {
                        DWORD val = *p;
                        if (val >= 0x00400000 && val <= 0x00C05000) {
                            RTLog::Write("  [stack+%03X] %08X (RVA %08X)",
                                (DWORD)((DWORD)p - myEsp), val, val - 0x00400000);
                            found++;
                        }
                    }
                    RTLog::Write("  --- end stack scan ---");
                    RTLog::Write("=== END CALLTRACE ===");
                }
            }

            UInt32& raceData = *reinterpret_cast<UInt32*>((char*)thisPtr + 0xE8);
            UInt32 savedRaceData = raceData;
            raceData = 0;
            s_origFiller(thisPtr, param1);
            raceData = savedRaceData;
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

// FUN_0043eb80 at 0x0043EB80: QueuedHelmet FaceGen processor, thiscall.
// Calls Filler directly + dispatches to ChokepointAlloc chain via sub_436F30.
// Diagnostic: probe in_ECX offsets to find NPC link.

typedef void (__thiscall *Fn_FUN_0043eb80)(void* thisPtr);
static Fn_FUN_0043eb80 s_origFUN_0043eb80 = nullptr;
static LONG s_43eb80Calls = 0;
static UInt32 s_cachedFormIDs[32] = {};
static LONG s_cacheCount = 0;
static CRITICAL_SECTION s_cacheCS;
static bool s_cacheInited = false;

static bool IsCached(UInt32 formID) {
    if (!s_cacheInited) { InitializeCriticalSection(&s_cacheCS); s_cacheInited = true; }
    EnterCriticalSection(&s_cacheCS);
    for (int i = 0; i < s_cacheCount; i++)
        if (s_cachedFormIDs[i] == formID) { LeaveCriticalSection(&s_cacheCS); return true; }
    if (s_cacheCount < 32)
        s_cachedFormIDs[s_cacheCount++] = formID;
    LeaveCriticalSection(&s_cacheCS);
    return false;
}

static void __fastcall Hooked_FUN_0043eb80(void* thisPtr, void* /*edx*/)
{
    __try {
        void* p20 = *(void**)((char*)thisPtr + 0x20);
        if (p20) {
            void* p150 = *(void**)((char*)p20 + 0x150);
            if (p150) {
                UInt32 formID = *reinterpret_cast<UInt32*>((char*)p150 + 0xC);
                if ((formID & 0xFF000000) == 0) {
                    void* mount = *(void**)((char*)p150 + 0x1A0);
                    if (mount) {
                        if (IsCached(formID)) {
                            LONG n = InterlockedIncrement(&s_43eb80Calls);
                            if (n <= 5 || n % 100 == 0)
                                RTLog::Write("F43EB80 SKIP formID=%08X (total=%d)", formID, n);
                            return;
                        }
                        // First call: let it through for FaceGen setup
                        RTLog::Write("F43EB80 CACHE formID=%08X (first call, allowing)", formID);
                    }
                }
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {}

    s_origFUN_0043eb80(thisPtr);
}

bool InstallFaceGenHooks()
{
    RTLog::Init();
    RTLog::Write("[INIT] InstallFaceGenHooks() entered");

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    s_origFiller = (Fn_FaceGenFiller)kAddr_TESNPC_FaceGenFiller;
    DetourAttach(&(PVOID&)s_origFiller, (PVOID)Hooked_TESNPC_FaceGenFiller);

    s_orig43b990 = (Fn_43b990)kAddr_43b990;
    DetourAttach(&(PVOID&)s_orig43b990, (PVOID)Hooked_43b990);

    s_origFUN_0043eb80 = (Fn_FUN_0043eb80)kAddr_FUN_0043eb80;
    DetourAttach(&(PVOID&)s_origFUN_0043eb80, (PVOID)Hooked_FUN_0043eb80);

    LONG result = DetourTransactionCommit();
    if (result != NO_ERROR) {
        _ERROR("DetourTransactionCommit failed: %d", result);
        return false;
    }

    _MESSAGE("FaceGenProbe initialized");
    _MESSAGE("  TESNPC_FaceGenFiller @ %08X (E8 zero/restore + caller logging)", kAddr_TESNPC_FaceGenFiller);
    _MESSAGE("  FUN_0043b990 @ %08X (creature mount redirect)", kAddr_43b990);
    RTLog::Write("[INIT] FaceGenProbe Build 35 (Filler + 43b990 + 43EB80 diagnostic)");

    return true;
}
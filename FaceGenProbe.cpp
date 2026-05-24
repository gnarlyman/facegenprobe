// Build 41: Deferred-free list validator + QueuedHelmet retire.
// Four hooks:
//   1. TESNPC_FaceGenFiller (0x005221C0): E8 zero/restore forces default race path
//   2. FUN_0043b990 (0x0043B990): Skips BSTask creation for creature mounts
//   3. FUN_0043eb80 (0x0043EB80): 2-call gate → counter=6 + sentinel in face0/face1
//   4. sub_4328B0  (0x004328B0): Validates LFM deferred-free list head, clears stale ptr
//
//   BUILD 42 CHANGE: Hook 3 now installs a fake BSFaceGenNiNode sentinel into
//   TESNPC.face0/face1 (+0x1D4/+0x1D8) when retiring a storm NPC. The sentinel has
//   a no-op vtable and sky-high refcount (0x40000000). The engine checks these
//   fields — if non-NULL, FaceGen is considered "done" and the NPC stops being
//   re-queued. Combined with counter=6, this stops the re-queue cycle at source.

#include "Detours/detours.h"
#include <cstdio>
#include <intrin.h>
#include <mutex>
#include <windows.h>

#pragma comment(lib, "Detours\\detours.lib")

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
static const UInt32 kAddr_FUN_0043eb80       = 0x0043EB80;
static const UInt32 kAddr_sub_4328B0         = 0x004328B0;
static const UInt32 kOblivionBase            = 0x00400000;

// -- Hook 1: TESNPC_FaceGenFiller (0x005221C0) --

typedef void (__thiscall *Fn_FaceGenFiller)(void* thisPtr, void* param1);
static Fn_FaceGenFiller s_origFiller = nullptr;

static UInt32 s_lastRetAddr[64] = {};
static UInt32 s_lastFormID[64] = {};
static int s_dedupIdx = 0;

static bool LogDedup(UInt32 formID, UInt32 retAddr) {
    for (int i = 0; i < s_dedupIdx; i++)
        if (s_lastFormID[i] == formID && s_lastRetAddr[i] == retAddr) return true;
    if (s_dedupIdx < 64) {
        s_lastFormID[s_dedupIdx] = formID;
        s_lastRetAddr[s_dedupIdx] = retAddr;
        s_dedupIdx++;
    }
    return false;
}

static void __fastcall Hooked_TESNPC_FaceGenFiller(void* thisPtr, void* /*edx*/, void* param1)
{
    if (!thisPtr) { s_origFiller(thisPtr, param1); return; }

    UInt32 formID = *reinterpret_cast<UInt32*>((char*)thisPtr + 0xC);
    if ((formID & 0xFF000000) == 0) {
        void* mount = *(void**)((char*)thisPtr + 0x1A0);
        if (mount) {
            UInt32 retAddr = (UInt32)_ReturnAddress();
            if (!LogDedup(formID, retAddr))
                RTLog::Write("[Filler] retAddr=%08X formID=%08X mount=%08X", retAddr, formID, (UInt32)mount);

            UInt32& raceData = *reinterpret_cast<UInt32*>((char*)thisPtr + 0xE8);
            UInt32 saved = raceData;
            raceData = 0;
            s_origFiller(thisPtr, param1);
            raceData = saved;
            return;
        }
    }
    s_origFiller(thisPtr, param1);
}

// -- Hook 2: FUN_0043b990 (0x0043B990) --

typedef UInt32 (__stdcall *Fn_43b990)(UInt32 p1, UInt32 p2, UInt32 p3, UInt32 p4, UInt32 mount);
static Fn_43b990 s_orig43b990 = nullptr;

static UInt32 __stdcall Hooked_43b990(UInt32 p1, UInt32 p2, UInt32 p3, UInt32 p4, UInt32 mount)
{
    if (mount != 0) {
        __try {
            UInt32 fgObj = *reinterpret_cast<UInt32*>(reinterpret_cast<char*>(mount) + 0x1C);
            if (fgObj == 0 || fgObj == 0xFFFFFFFF) { *reinterpret_cast<UInt32*>(p1) = 0; return p1; }
            UInt32 vtbl = *reinterpret_cast<UInt32*>(reinterpret_cast<char*>(fgObj));
            UInt32 fn   = *reinterpret_cast<UInt32*>(reinterpret_cast<char*>(vtbl) + 0xF4);
            typedef char (__thiscall *FnCheck)(void*);
            if (reinterpret_cast<FnCheck>(fn)(reinterpret_cast<void*>(fgObj)) == 0)
                { *reinterpret_cast<UInt32*>(p1) = 0; return p1; }
        } __except(EXCEPTION_EXECUTE_HANDLER) {}
    }
    return s_orig43b990(p1, p2, p3, p4, mount);
}

// -- Hook 3: FUN_0043eb80 (0x0043EB80) — 2-call gate → counter=6 + sentinel --

// FaceGen sentinel: fake BSFaceGenNiNode with no-op vtable, installed in NPC's
// face0/face1 to convince the engine FaceGen is done. Refcount 0x40000000 (~1B)
// ensures InterlockedDecrement never reaches 0. Vtable entries do `ret 4` (safe
// for 1-arg __thiscall dtors and most BSFaceGenNiNode virtual methods).
#pragma pack(push, 4)
struct FaceGenSentinel { void* vtbl; LONG refcount; UInt8 pad[0x200]; };
#pragma pack(pop)
static FaceGenSentinel g_sentinel;
static void* g_sentinelVtbl[64];

__declspec(naked) static void SentinelThunkRet4() { __asm { ret 4 } }

static void InitSentinel() {
    memset(&g_sentinel, 0, sizeof(g_sentinel));
    g_sentinelVtbl[0] = (void*)&SentinelThunkRet4;
    for (int i = 1; i < 64; i++) g_sentinelVtbl[i] = (void*)&SentinelThunkRet4;
    g_sentinel.vtbl = g_sentinelVtbl;
    g_sentinel.refcount = 0x40000000;
}

typedef void (__thiscall *Fn_FUN_0043eb80)(void* thisPtr);
static Fn_FUN_0043eb80 s_origFUN_0043eb80 = nullptr;
static LONG s_retireCount = 0;

static UInt32 s_cachedFormIDs[32] = {};
static LONG s_cacheCount = 0;
static CRITICAL_SECTION s_cacheCS;
static bool s_cacheInited = false;

static bool ShouldRetire(UInt32 formID) {
    if (!s_cacheInited) { InitializeCriticalSection(&s_cacheCS); s_cacheInited = true; }
    static LONG s_formCounters[32] = {};
    EnterCriticalSection(&s_cacheCS);
    for (int i = 0; i < s_cacheCount; i++) {
        if (s_cachedFormIDs[i] == formID) {
            LONG n = InterlockedIncrement(&s_formCounters[i]);
            LeaveCriticalSection(&s_cacheCS);
            return n > 2;
        }
    }
    if (s_cacheCount < 32) {
        s_cachedFormIDs[s_cacheCount] = formID;
        s_formCounters[s_cacheCount] = 1;
        s_cacheCount++;
    }
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
                        if (ShouldRetire(formID)) {
                            LONG n = InterlockedIncrement(&s_retireCount);
                            if (n <= 5 || n % 100 == 0)
                                RTLog::Write("[Retire] formID=%08X sentinel+6 (total=%d)", formID, n);
                            *reinterpret_cast<int*>((char*)thisPtr + 0x0C) = 6;
                            void** face0 = (void**)((char*)p150 + 0x1D4);
                            void** face1 = (void**)((char*)p150 + 0x1D8);
                            __try {
                                if (*face0 == nullptr) *face0 = &g_sentinel;
                                if (*face1 == nullptr) *face1 = &g_sentinel;
                            } __except(EXCEPTION_EXECUTE_HANDLER) {}
                            s_origFUN_0043eb80(thisPtr);
                            return;
                        }
                    }
                }
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {}

    s_origFUN_0043eb80(thisPtr);
}

// -- Hook 4: sub_4328B0 (0x004328B0) — LFM deferred-free list validator --

typedef void (__thiscall *Fn_sub_4328B0)(void* thisPtr);
static Fn_sub_4328B0 s_orig_sub_4328B0 = nullptr;
static LONG s_lfmClearCount = 0;

static void __fastcall Hooked_sub_4328B0(void* thisPtr, void* /*edx*/)
{
    void* head = *(void**)((char*)thisPtr + 0x1C);
    if (head) {
        MEMORY_BASIC_INFORMATION mbi = {};
        if (!VirtualQuery(head, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT) {
            *(void**)((char*)thisPtr + 0x1C) = nullptr;
            LONG n = InterlockedIncrement(&s_lfmClearCount);
            if (n <= 5 || n % 100 == 0)
                RTLog::Write("[LFM] cleared stale deferred-free head %p (total=%d)", head, n);
        }
    }
    s_orig_sub_4328B0(thisPtr);
}

// -- Install --

bool InstallFaceGenHooks()
{
    RTLog::Init();
    RTLog::Write("[INIT] FaceGenProbe Build 42");
    InitSentinel();

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    s_origFiller = (Fn_FaceGenFiller)kAddr_TESNPC_FaceGenFiller;
    DetourAttach(&(PVOID&)s_origFiller, (PVOID)Hooked_TESNPC_FaceGenFiller);

    s_orig43b990 = (Fn_43b990)kAddr_43b990;
    DetourAttach(&(PVOID&)s_orig43b990, (PVOID)Hooked_43b990);

    s_origFUN_0043eb80 = (Fn_FUN_0043eb80)kAddr_FUN_0043eb80;
    DetourAttach(&(PVOID&)s_origFUN_0043eb80, (PVOID)Hooked_FUN_0043eb80);

    s_orig_sub_4328B0 = (Fn_sub_4328B0)kAddr_sub_4328B0;
    DetourAttach(&(PVOID&)s_orig_sub_4328B0, (PVOID)Hooked_sub_4328B0);

    LONG result = DetourTransactionCommit();
    if (result != NO_ERROR) {
        _ERROR("DetourTransactionCommit failed: %d", result);
        return false;
    }

    _MESSAGE("FaceGenProbe initialized");
    _MESSAGE("  TESNPC_FaceGenFiller @ %08X (E8 zero/restore)", kAddr_TESNPC_FaceGenFiller);
    _MESSAGE("  FUN_0043b990 @ %08X (creature mount redirect)", kAddr_43b990);
    _MESSAGE("  FUN_0043eb80 @ %08X (2-call gate → retire)", kAddr_FUN_0043eb80);
    _MESSAGE("  sub_4328B0 @ %08X (LFM deferred-free list validator)", kAddr_sub_4328B0);

    return true;
}

#pragma once
// Self-managed real-time log file, mirroring g5's proven approach:
// resolve the dir THIS DLL lives in (USVFS redirects writes there to
// Reborn\overwrite\OBSE\Plugins\), open once with "w", write an immediate
// "opened" line so the file exists from game start (provable without a
// storm), keep the handle open, fflush every write so it's tail-able live.
// Oblivion's console + ConScribe are NOT real-time; this file is.
#include <windows.h>
#include <cstdio>
#include <share.h>     // _SH_DENYWR — allow other processes to READ while we write
#include <cstdarg>
#include <ctime>
#include <mutex>

namespace StormLog { namespace Lg {

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
    if (snprintf(full, sizeof(full), "%s\\storm_report.log", dllPath) <= 0) return;
    // _fsopen with _SH_DENYWR: WE keep exclusive write, but OTHER processes
    // may open the file for READ while the game runs (tail/editor/scripts).
    // Plain fopen/fopen_s denies all sharing -> ERROR_SHARING_VIOLATION for
    // any reader. (g5 never hit this: its live channel is a socket, not the
    // log file.)
    Fp() = _fsopen(full, "w", _SH_DENYWR);
    if (Fp()) {
        fprintf(Fp(), "=== StormLog storm_report.log opened (%s) ===\n", full);
        fflush(Fp());
    }
}

}} // namespace StormLog::Lg

#pragma once
#include <cstdint>

#if defined(_WIN32)
  #include <windows.h>
  namespace StormLog {
      inline uint64_t TimeMs() { return ::GetTickCount64(); }
  }
#else
  #include <chrono>
  namespace StormLog {
      inline uint64_t TimeMs() {
          using namespace std::chrono;
          return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
      }
  }
#endif

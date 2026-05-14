#pragma once
#include <cstdint>
#include <cstddef>

namespace StormLog {

enum class CsvLayer     : uint8_t { L1 = 0, L3a = 1, L3b = 2 };
enum class CsvEventType : uint8_t { FIRST_SEEN = 0, BURST_END, STORM, L3_DISPATCH };

struct CsvEvent {
    uint64_t      time_ms;
    CsvLayer      layer;
    CsvEventType  event_type;
    uint32_t      npc_formid;
    const char*   npc_edid;       // borrowed; nullable
    const char*   mod_source;     // borrowed; nullable
    uint32_t      cell_formid;
    uint32_t      burst_count;
    uint32_t      calls_in_window;
    uint32_t      total_calls_l1;
    uint32_t      total_retries_l1;
    uint32_t      total_l3;
};

namespace CsvWriter {
    const char* Header();
    const char* LayerStr(CsvLayer);
    const char* EventTypeStr(CsvEventType);

    // Formats one CSV row (including trailing newline) into buf.
    // Returns number of chars written (excluding final null), or 0 if buf too small.
    size_t FormatRow(const CsvEvent& e, char* buf, size_t buflen);
}

} // namespace StormLog

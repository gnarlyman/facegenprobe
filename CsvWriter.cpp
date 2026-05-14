#include "CsvWriter.h"
#include <cstdio>
#include <cstring>

namespace StormLog {

const char* CsvWriter::Header() {
    return "time_ms,layer,event_type,npc_formid,npc_edid,mod_source,cell_formid,"
           "burst_count,calls_in_window,total_calls_l1,total_retries_l1,total_l3\n";
}

const char* CsvWriter::LayerStr(CsvLayer l) {
    switch (l) {
        case CsvLayer::L1:  return "L1";
        case CsvLayer::L3a: return "L3a";
        case CsvLayer::L3b: return "L3b";
    }
    return "?";
}

const char* CsvWriter::EventTypeStr(CsvEventType e) {
    switch (e) {
        case CsvEventType::FIRST_SEEN:  return "FIRST_SEEN";
        case CsvEventType::BURST_END:   return "BURST_END";
        case CsvEventType::STORM:       return "STORM";
        case CsvEventType::L3_DISPATCH: return "L3_DISPATCH";
    }
    return "?";
}

// Per RFC 4180, embedded newlines in quoted fields are preserved as-is — this is
// legal CSV but line-oriented readers (getline) will split rows. For StormLog's
// diagnostic CSVs consumed by Python/Excel/pandas, this is acceptable.
//
// Write s to out, escaping if it contains , " or newline.
// Returns number of chars written.
static size_t EscapeField(const char* s, char* out, size_t cap) {
    if (!s) s = "";
    bool needsQuote = false;
    for (const char* p = s; *p; ++p) {
        if (*p == ',' || *p == '"' || *p == '\n' || *p == '\r') { needsQuote = true; break; }
    }
    size_t n = 0;
    if (needsQuote) {
        if (n + 1 >= cap) return 0;
        out[n++] = '"';
    }
    for (const char* p = s; *p; ++p) {
        if (*p == '"') {
            if (n + 2 >= cap) return 0;
            out[n++] = '"'; out[n++] = '"';
        } else {
            if (n + 1 >= cap) return 0;
            out[n++] = *p;
        }
    }
    if (needsQuote) {
        if (n + 1 >= cap) return 0;
        out[n++] = '"';
    }
    return n;
}

size_t CsvWriter::FormatRow(const CsvEvent& e, char* buf, size_t buflen) {
    // edidEsc 128 = 4x typical Oblivion EDID cap (~32 chars); modEsc 64 covers ESP
    // filenames + worst-case quote-escaping.
    char edidEsc[128];
    char modEsc[64];
    size_t edidN = EscapeField(e.npc_edid,   edidEsc, sizeof(edidEsc));
    size_t modN  = EscapeField(e.mod_source, modEsc,  sizeof(modEsc));
    if ((e.npc_edid   && *e.npc_edid   && edidN == 0) ||
        (e.mod_source && *e.mod_source && modN  == 0)) {
        return 0; // escape buffer overflow
    }
    edidEsc[edidN] = 0;
    modEsc[modN]   = 0;

    int n = std::snprintf(buf, buflen,
        "%llu,%s,%s,%08X,%s,%s,%08X,%u,%u,%u,%u,%u\n",
        (unsigned long long)e.time_ms,
        LayerStr(e.layer),
        EventTypeStr(e.event_type),
        e.npc_formid,
        edidEsc,
        modEsc,
        e.cell_formid,
        e.burst_count,
        e.calls_in_window,
        e.total_calls_l1,
        e.total_retries_l1,
        e.total_l3);
    if (n < 0 || (size_t)n >= buflen) return 0;
    return (size_t)n;
}

} // namespace StormLog

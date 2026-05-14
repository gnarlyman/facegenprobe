#include "TestFramework.h"
#include "../CsvWriter.h"
#include <string>

using namespace StormLog;

TEST(csv_header_is_canonical) {
    ASSERT_STREQ(CsvWriter::Header(),
        "time_ms,layer,event_type,npc_formid,npc_edid,mod_source,cell_formid,burst_count,calls_in_window,total_calls_l1,total_retries_l1,total_l3\n");
}

TEST(csv_row_basic) {
    CsvEvent e{};
    e.time_ms          = 12345;
    e.layer            = CsvLayer::L1;
    e.event_type       = CsvEventType::BURST_END;
    e.npc_formid       = 0x000ABCDE;
    e.npc_edid         = "BanditBoss";
    e.mod_source       = "OOO.esp";
    e.cell_formid      = 0x00012345;
    e.burst_count      = 7;
    e.calls_in_window  = 12;
    e.total_calls_l1   = 100;
    e.total_retries_l1 = 30;
    e.total_l3         = 5;

    char buf[512];
    size_t n = CsvWriter::FormatRow(e, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);
    ASSERT_STREQ(buf,
        "12345,L1,BURST_END,000ABCDE,BanditBoss,OOO.esp,00012345,7,12,100,30,5\n");
}

TEST(csv_row_escapes_quotes_and_commas) {
    CsvEvent e{};
    e.time_ms     = 1;
    e.layer       = CsvLayer::L1;
    e.event_type  = CsvEventType::FIRST_SEEN;
    e.npc_formid  = 0x1;
    e.npc_edid    = "weird,name\"with\"quotes";
    e.mod_source  = "M.esp";
    e.cell_formid = 0;

    char buf[512];
    CsvWriter::FormatRow(e, buf, sizeof(buf));
    // RFC 4180: surrounding quotes when field contains , or "; embedded " is doubled.
    ASSERT_STREQ(buf,
        "1,L1,FIRST_SEEN,00000001,\"weird,name\"\"with\"\"quotes\",M.esp,00000000,0,0,0,0,0\n");
}

TEST(csv_layer_and_event_type_strings) {
    ASSERT_STREQ(CsvWriter::LayerStr(CsvLayer::L1),  "L1");
    ASSERT_STREQ(CsvWriter::LayerStr(CsvLayer::L3a), "L3a");
    ASSERT_STREQ(CsvWriter::LayerStr(CsvLayer::L3b), "L3b");
    ASSERT_STREQ(CsvWriter::EventTypeStr(CsvEventType::FIRST_SEEN),   "FIRST_SEEN");
    ASSERT_STREQ(CsvWriter::EventTypeStr(CsvEventType::BURST_END),    "BURST_END");
    ASSERT_STREQ(CsvWriter::EventTypeStr(CsvEventType::STORM),        "STORM");
    ASSERT_STREQ(CsvWriter::EventTypeStr(CsvEventType::L3_DISPATCH),  "L3_DISPATCH");
}

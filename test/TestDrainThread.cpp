#include "TestFramework.h"
#include "../DrainThread.h"
#include "../RingBuffer.h"
#include "../CsvWriter.h"
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include <windows.h>

using namespace StormLog;

static std::string MakeTempPath(const char* suffix) {
    char tmpDir[260], tmpFile[260];
    GetTempPathA(sizeof(tmpDir), tmpDir);
    GetTempFileNameA(tmpDir, "sld", 0, tmpFile);
    std::remove(tmpFile);
    return std::string(tmpFile) + suffix;
}

static std::string SlurpFile(const std::string& path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

TEST(drain_writes_header_and_rows) {
    std::string path = MakeTempPath(".csv");
    RingBuffer rb(4096);
    DrainThread d(&rb, path.c_str(), /*flushIntervalMs*/100, /*flushEveryN*/2);

    d.Start();
    rb.Push("row1\n", 5);
    rb.Push("row2\n", 5);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    d.Stop();

    std::string s = SlurpFile(path);
    ASSERT_TRUE(s.find(CsvWriter::Header()) == 0);
    ASSERT_TRUE(s.find("row1\nrow2\n") != std::string::npos);

    std::remove(path.c_str());
}

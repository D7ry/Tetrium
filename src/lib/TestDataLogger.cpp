#include "TestDataLogger.h"
#include "PCH.h"
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace TetriumApp
{
namespace
{
std::string EscapeCsvValue(const std::string& value)
{
    bool needsQuotes = value.find_first_of(",\"\n\r") != std::string::npos;
    if (!needsQuotes) {
        return value;
    }

    std::string escaped = "\"";
    for (char c : value) {
        if (c == '"') {
            escaped += "\"\"";
        } else {
            escaped += c;
        }
    }
    escaped += "\"";
    return escaped;
}
} // namespace

TestDataLogger::TestDataLogger(
    const std::string& appName,
    const std::string& subjectId,
    const std::vector<std::string>& columnHeaders,
    const std::string& additionalInfo
)
    : _headers(columnHeaders), _headerWritten(false)
{
    // Create data directory structure in project root: ../data/<appName>/
    // When running from build/, this goes up one level to project root
    std::string dataDir = "../data/" + appName;
    std::filesystem::create_directories(dataDir);

    // Generate filename with timestamp
    std::string timestamp = getCurrentTimestamp();
    std::string filenameBase = subjectId;
    if (!additionalInfo.empty()) {
        filenameBase += "_" + additionalInfo;
    }
    _filePath = dataDir + "/" + filenameBase + "_" + timestamp + ".csv";

    // Open file
    _file.open(_filePath, std::ios::out | std::ios::trunc);
    if (!_file.is_open()) {
        ERROR("Failed to open CSV file: {}", _filePath);
        return;
    }

    writeHeader();
    INFO("TestDataLogger initialized: {}", _filePath);
}

TestDataLogger::~TestDataLogger()
{
    if (_file.is_open()) {
        _file.close();
    }
    INFO("TestDataLogger closed: {}", _filePath);
}

void TestDataLogger::writeHeader()
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (_headerWritten || !_file.is_open())
        return;

    for (size_t i = 0; i < _headers.size(); ++i) {
        _file << _headers[i];
        if (i < _headers.size() - 1)
            _file << ",";
    }
    _file << "\n";
    _file.flush();
    _headerWritten = true;
}

void TestDataLogger::LogRow(const std::map<std::string, std::string>& data)
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (!_file.is_open()) {
        ERROR("Cannot log row - file not open");
        return;
    }

    for (size_t i = 0; i < _headers.size(); ++i) {
        auto it = data.find(_headers[i]);
        if (it != data.end()) {
            _file << EscapeCsvValue(it->second);
        }
        if (i < _headers.size() - 1)
            _file << ",";
    }
    _file << "\n";
    _file.flush();
}

std::string TestDataLogger::getCurrentTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
    ss << "_" << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

} // namespace TetriumApp

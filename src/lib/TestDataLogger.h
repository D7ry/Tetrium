#pragma once

#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace TetriumApp
{

class TestDataLogger
{
  public:
    TestDataLogger(
        const std::string& appName,
        const std::string& subjectId,
        const std::vector<std::string>& columnHeaders
    );

    ~TestDataLogger();

    // Log a row of data to the CSV file
    void LogRow(const std::map<std::string, std::string>& data);

    // Get the output file path
    std::string GetFilePath() const { return _filePath; }

    static std::string getCurrentTimestamp();

  private:
    std::string _filePath;
    std::vector<std::string> _headers;
    std::ofstream _file;
    std::mutex _mutex;
    bool _headerWritten;

    void writeHeader();
};

} // namespace TetriumApp

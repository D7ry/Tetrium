#pragma once
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

// NOTE: following must be inlined to avoid static initialization order fiasco
#if defined(_WIN32)
inline const std::string ASSETS_PATH = "../../assets/";
inline const std::string TETRIUM_COLOR_PATH = "../../extern/TetriumColor/";
#else
inline const std::string ASSETS_PATH = "../assets/";
inline const std::string TETRIUM_COLOR_PATH = "../extern/TetriumColor/";
#endif // WIN32

inline std::string getCurrentDate()
{
    // Get the current time as a time_point
    auto now = std::chrono::system_clock::now();

    // Convert to time_t to obtain the time in seconds
    std::time_t t = std::chrono::system_clock::to_time_t(now);

    // Convert to tm struct for formatting
    std::tm tm = *std::localtime(&t);

    // Create a string stream to format the date
    std::stringstream ss;

    // Format as YYYY-MM-DD
    ss << std::put_time(&tm, "%Y-%m-%d");

    return ss.str();
}

inline std::string getTodayPrimariesPath()
{
    std::string date = getCurrentDate();
    return TETRIUM_COLOR_PATH + "measurements/" + date + "/primaries";
}

inline std::string getTodayMeasurementsPath()
{
    std::string date = getCurrentDate();
    return TETRIUM_COLOR_PATH + "measurements/" + date;
}

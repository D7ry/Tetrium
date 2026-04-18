#pragma once

#include <string>
#include <vector>
#include <utility>

// https://github.com/berkeley-oz-vision/SixPrimaryProjector/blob/main/LedDriverGUI/devices/PR650.py
class PR650 {
public:
    explicit PR650(const std::string& portName);
    ~PR650();

    // Integration time per exposure in ms ('s' command field AAAA). Must be >= 34 ms to
    // cover one full 30 Hz stimulus period (33.33 ms) and avoid aliasing. Set to 0 for
    // adaptive (PR650 chooses automatically, which may be too short to avoid aliasing).
    int exposureTimeMs = 100;

    // Number of exposures averaged per measurement ('s' command field BB).
    // Sync mode is free-run (0), so exposures are not phase-locked to the display.
    // With exposureTimeMs >= 34, aliasing is already eliminated; increase numExposures
    // only if additional noise reduction is needed, at the cost of proportionally longer
    // measurement time.
    int numExposures = 1;

    void Init();

    bool isConnected() const;

    /// Measures luminance (lux or cd/m^2 depending on device config)
    double measureLum();

    /// Measures the full spectrum (returns wavelength, power) and luminance
    void StartMeasuring();

    /// Gets the last measured luminance without re-measuring
    double getLum() const;

    struct SpectrumMeasure {
        std::vector <double> wavelength;
        std::vector <double> power;
        double luminance;
        std::atomic<bool> ready = false;
    } MeasureResult;

private:
    bool sendMessage(const std::string& message, std::string& response, int timeout);
    bool sendMessageMultiLine(const std::string& message, std::vector<std::string>& responseLines, int timeout);

    void parseSpectrumOutput(const std::vector<std::string>& raw,
                             std::vector<double>& wavelengths,
                             std::vector<double>& powers);

    bool initConnection();
    void closeConnection();

    std::string portName_;
#if defined(WIN32)
    void* serialHandle_;
#else
    int serialHandle_;
#endif
    bool serialConnected_ = false;
    bool connected_;
    double lum_;

    const std::string OK_CODE = "000";

};

#pragma once

#include <string>
#include <vector>
#include <utility>

// https://github.com/berkeley-oz-vision/SixPrimaryProjector/blob/main/LedDriverGUI/devices/PR650.py
class PR650 {
public:
    explicit PR650(const std::string& portName);
    ~PR650();

    // Number of exposures averaged per measurement (passed to PR650 's' command field BB).
    // Sync mode is set to free-run (0) so measurements are NOT phase-locked to the display
    // VSync. This prevents systematic aliasing with a 30 Hz stimulus on a 60 Hz display,
    // where sync mode 1 would always sample the same half of the 30 Hz cycle.
    // With free-run, each exposure lands at an arbitrary phase, so even numExposures=1
    // has no systematic aliasing bias. Increase only if you need lower noise at the cost
    // of proportionally longer measurement time.
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

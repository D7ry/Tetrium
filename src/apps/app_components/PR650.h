#pragma once

#include <string>
#include <vector>
#include <utility>

// https://github.com/berkeley-oz-vision/SixPrimaryProjector/blob/main/LedDriverGUI/devices/PR650.py
class PR650 {
public:
    explicit PR650(const std::string& portName);
    ~PR650();

    void Init();

    bool isConnected() const;

    /// Measures luminance (lux or cd/m^2 depending on device config)
    double measureLum();

    /// Measures the full spectrum (returns wavelength, power) and luminance
    void measureSpectrum();

    /// Gets the last measured luminance without re-measuring
    double getLum() const;

    struct SpectrumMeasure {
        std::vector <double> wavelength;
        std::vector <double> power;
        double luminance;
        std::atomic<bool> ready = false;
    } MeasureResult;

private:
    bool sendMessage(const std::string& message, std::string& response, int timeout = 30);
    bool sendMessageMultiLine(const std::string& message, std::vector<std::string>& responseLines, int timeout = 30);

    void parseSpectrumOutput(const std::vector<std::string>& raw,
                             std::vector<double>& wavelengths,
                             std::vector<double>& powers);

    bool initConnection();
    void closeConnection();

    std::string portName_;
    void* serialHandle_;
    bool serialConnected_ = false;
    bool connected_;
    double lum_;

    const std::string OK_CODE = "000";

};

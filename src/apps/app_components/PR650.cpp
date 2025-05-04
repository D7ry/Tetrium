#include "PR650.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <cstring>
#include <utility>
#include <string>

#if defined(WIN32)
    #include <windows.h>
#else
    #include <fcntl.h>
    #include <termios.h>
    #include <unistd.h>
#endif

void PR650::Init() {
    connected_ = initConnection();
    DEBUG("serial port connected!");
    if (connected_) {
        std::string reply;
        // sleep for 500ms
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if (!sendMessage("b1", reply)) {
            ERROR("failed to turn on PR650 backlight");
            connected_ = false;
        } else {
            INFO("PR650 connected on {}, backlight on", portName_);
            sendMessage("s01,,,,,,01,1", reply);
        }
    }
}
PR650::PR650(const std::string& portName)
    : portName_(portName), lum_(0.0), connected_(false) {
}

PR650::~PR650() {
    closeConnection();
}

bool PR650::initConnection() {
#if defined(WIN32)
    serialHandle_ = CreateFileA(portName_.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (serialHandle_ == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open serial port (Windows)\n";
        return false;
    }

    DCB dcbSerialParams = { 0 };
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

    if (!GetCommState(serialHandle_, &dcbSerialParams)) {
        std::cerr << "Failed to get COM state.\n";
        return false;
    }

    dcbSerialParams.BaudRate = CBR_9600;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity   = NOPARITY;

    if (!SetCommState(serialHandle_, &dcbSerialParams)) {
        std::cerr << "Could not set serial port parameters\n";
        return false;
    }

    COMMTIMEOUTS timeouts = { 0 };
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;

    SetCommTimeouts(serialHandle_, &timeouts);
#else
    serialHandle_ = open(portName_.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if (serialHandle_ < 0) {
        perror("Error opening serial port");
        return false;
    }

    termios tty{};
    if (tcgetattr(serialHandle_, &tty) != 0) {
        perror("tcgetattr");
        return false;
    }

    cfsetospeed(&tty, B9600);
    cfsetispeed(&tty, B9600);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 5;

    if (tcsetattr(serialHandle_, TCSANOW, &tty) != 0) {
        perror("tcsetattr");
        return false;
    }
#endif
    return true;
}

void PR650::closeConnection() {
#if defined(WIN32)
    if (serialHandle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(serialHandle_);
    }
#else
    if (serialHandle_ >= 0) {
        close(serialHandle_);
    }
#endif
}

bool PR650::isConnected() const {
    return connected_;
}

bool PR650::sendMessage(const std::string& message, std::string& response, int timeout) {
#if defined(WIN32)
    DWORD bytesWritten;
    std::string msg = message.back() == '\n' ? message : message + "\n";
    if (!WriteFile(serialHandle_, msg.c_str(), msg.length(), &bytesWritten, NULL)) {
        std::cerr << "Failed to write to port.\n";
        return false;
    }

    Sleep(timeout); // milliseconds

    char buffer[256];
    DWORD bytesRead;
    if (!ReadFile(serialHandle_, buffer, sizeof(buffer), &bytesRead, NULL)) {
        std::cerr << "Failed to read from port.\n";
        return false;
    }
    response = std::string(buffer, bytesRead);
#else
    std::string msg = message.back() == '\n' ? message : message + "\n";
    write(serialHandle_, msg.c_str(), msg.length());
    tcdrain(serialHandle_);
    usleep(timeout * 1000);

    char buf[256] = {0};
    int n = read(serialHandle_, buf, sizeof(buf));
    if (n <= 0) return false;
    response = std::string(buf, n);
#endif
    return true;
}

bool PR650::sendMessageMultiLine(const std::string& message, std::vector<std::string>& responseLines, int timeout) {
    std::string fullResponse;
    if (!sendMessage(message, fullResponse, timeout)) return false;

    std::istringstream iss(fullResponse);
    std::string line;
    while (std::getline(iss, line)) {
        responseLines.push_back(line);
    }
    return true;
}

double PR650::measureLum() {
    std::string response;
    if (!sendMessage("m0", response)) {
        std::cerr << "Measurement failed.\n";
        lum_ = 0.0;
        return lum_;
    }

    if (response.find(OK_CODE) != std::string::npos) {
        if (sendMessage("d2", response)) {
            std::istringstream ss(response);
            std::string val;
            int idx = 0;
            while (std::getline(ss, val, ',')) {
                if (idx == 3) {
                    lum_ = std::stod(val);
                    break;
                }
                ++idx;
            }
        }
    } else {
        lum_ = 0.0;
    }
    return lum_;
}

void PR650::measureSpectrum() {
    this->MeasureResult.ready = false;
    measureLum();
    std::vector<std::string> raw;
    std::vector<double> nm, power;
    sendMessageMultiLine("d5", raw);
    parseSpectrumOutput(raw, nm, power);
    
    this->MeasureResult.wavelength = nm;
    this->MeasureResult.power = power;
    this->MeasureResult.luminance = lum_;
    this->MeasureResult.ready = true;
}

void PR650::parseSpectrumOutput(const std::vector<std::string>& raw,
                                std::vector<double>& wavelengths,
                                std::vector<double>& powers) {
    if (lum_ == 0.0) {
        wavelengths.clear();
        powers.assign(raw.size() - 2, 0.0);
        return;
    }

    for (size_t i = 2; i < raw.size(); ++i) {
        std::istringstream ss(raw[i]);
        std::string nmStr, powStr;
        // using ',' as delimiter to split each line
        if (std::getline(ss, nmStr, ',') && std::getline(ss, powStr)) {
            wavelengths.push_back(std::stod(nmStr));
            powers.push_back(std::stod(powStr));
        } else {
            PANIC("failed to split line: {}", std::string(raw[i]));
        }
    }
}

double PR650::getLum() const {
    return lum_;
}

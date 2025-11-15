#include "PR650.h"
#include <chrono>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#if defined(WIN32)
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

// NOTE: not reentrant
void PR650::Init()
{
    if (!serialConnected_) {
        if (initConnection()) {
            serialConnected_ = true;
        } else {
            PANIC("Failed to connect to serial port {}", portName_);
        }
    }

    std::string reply;

    while (reply != "000\r\n") {
        ERROR("reply {}, not 000, trying again", reply);
        ASSERT(sendMessage("b1", reply, 1000));
    }

    INFO("PR650 connected on {}, backlight on: {}", portName_, reply);
    ASSERT(sendMessage("s01,,,,,,01,1", reply, 1000));
    INFO("response to some shit we sent: {}", reply);
    connected_ = true;
}

PR650::PR650(const std::string& portName) : portName_(portName), lum_(0.0), connected_(false) {}

PR650::~PR650() { closeConnection(); }

bool PR650::initConnection()
{
#if defined(WIN32)
    serialHandle_ = CreateFileA(
        portName_.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL
    );
    if (serialHandle_ == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open serial port (Windows)\n";
        return false;
    }

    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

    if (!GetCommState(serialHandle_, &dcbSerialParams)) {
        std::cerr << "Failed to get COM state.\n";
        return false;
    }

    dcbSerialParams.BaudRate = CBR_9600;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;

    if (!SetCommState(serialHandle_, &dcbSerialParams)) {
        std::cerr << "Could not set serial port parameters\n";
        return false;
    }

    COMMTIMEOUTS timeouts = {0};
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

void PR650::closeConnection()
{
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

bool PR650::isConnected() const { return serialConnected_ && connected_; }

bool PR650::sendMessage(const std::string& message, std::string& response, int timeout)
{
#if defined(WIN32)
    DWORD bytesWritten;
    std::string msg = message.back() == '\n' ? message : message + "\n";
    if (!WriteFile(serialHandle_, msg.c_str(), msg.length(), &bytesWritten, NULL)) {
        std::cerr << "Failed to write to port.\n";
        return false;
    }

    // const int WAIT_REACT_TIMME = 1000; // wait for 1000 ms for PR650 to clear the IO buffer.
    // Sleep(WAIT_REACT_TIMME);           // milliseconds

    char* buf = (char*)malloc(1024);
    DWORD bytesRead;
    std::vector<char> totalData;
    auto start = std::chrono::steady_clock::now();

    while (true) {
        if (!ReadFile(serialHandle_, buf, 1024, &bytesRead, NULL)) {
            DWORD error = GetLastError();
            if (error == ERROR_HANDLE_EOF) {
                INFO("End of file reached");
                break;
            } else {
                PANIC("Failed to read from port. Error code: {}", error);
                return false;
            }
        }

        if (bytesRead > 0) {
            INFO("Read {} bytes", bytesRead);
            totalData.insert(totalData.end(), buf, buf + bytesRead);
        } else {
            INFO("read 0 bytes");
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - start
        )
                           .count();

        if (elapsed > timeout) {
            break;
        }

        // std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    delete buf;

    response = std::string(totalData.data(), totalData.size());
    INFO("response: {}", response);
#else
    // Flush any pending input
    tcflush(serialHandle_, TCIFLUSH);

    std::string msg = message.back() == '\n' ? message : message + "\n";
    ssize_t bytesWritten = write(serialHandle_, msg.c_str(), msg.length());
    if (bytesWritten < 0) {
        perror("write");
        return false;
    }

    tcdrain(serialHandle_); // Wait for all data to be transmitted
    usleep(500000);         // Allow PR650 to process (500ms delay like Python version)

    // Read until newline or timeout
    auto start = std::chrono::steady_clock::now();
    std::vector<char> totalData;
    char buf[256];

    while (true) {
        int n = read(serialHandle_, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            totalData.insert(totalData.end(), buf, buf + n);

            // Check if we got a complete line (ends with \r\n or \n)
            if (totalData.size() >= 1 && totalData.back() == '\n') {
                break;
            }
        } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("read");
            return false;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - start
        )
                           .count();
        if (elapsed > timeout) {
            break;
        }

        usleep(10000); // Small delay to avoid busy waiting
    }

    if (totalData.empty()) {
        return false;
    }

    response = std::string(totalData.data(), totalData.size());
    INFO("response: {}", response);
#endif
    return true;
}

bool PR650::sendMessageMultiLine(
    const std::string& message,
    std::vector<std::string>& responseLines,
    int timeout
)
{
    std::string fullResponse;
    if (!sendMessage(message, fullResponse, timeout))
        return false;

    std::istringstream iss(fullResponse);
    std::string line;
    while (std::getline(iss, line)) {
        responseLines.push_back(line);
    }
    return true;
}

double PR650::measureLum()
{
    std::string response;
    INFO("sending m0");
    ASSERT(sendMessage("m0", response, 30 * 1000)) // wait 10 seconds

    if (response.find(OK_CODE) != std::string::npos) {
        INFO("measuring success!");
        if (sendMessage("d2", response, 30 * 1000)) { // expecting 10 seconds luminance measuring
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
            if (idx != 3) {
                PANIC("less than 3 values returned from luminance measure result! : {}", response);
            }
        }
    } else {
        ERROR("response OK code not found: {}. {}", response, response.size());
        lum_ = 0.0;
    }
    return lum_;
}

void PR650::StartMeasuring()
{
    this->MeasureResult.ready = false;
    measureLum();
    INFO("luminance measuring success:  {}", lum_);
    std::vector<std::string> raw;
    std::vector<double> nm, power;
    sendMessageMultiLine("d5", raw, 30 * 1000);
    INFO("sent spectrum measurement");
    parseSpectrumOutput(raw, nm, power);
    INFO("specturm measuring success");

    this->MeasureResult.wavelength = nm;
    this->MeasureResult.power = power;
    this->MeasureResult.luminance = lum_;
    this->MeasureResult.ready = true;
}

void PR650::parseSpectrumOutput(
    const std::vector<std::string>& raw,
    std::vector<double>& wavelengths,
    std::vector<double>& powers
)
{
    INFO("all lines:");
    for (auto& line : raw) {
        INFO("{}", line);
    }
    if (lum_ == 0.0) {
        wavelengths.clear();
        powers.assign(raw.size() - 2, 0.0);
        return;
    }
    INFO("1");

    for (size_t i = 2; i < raw.size(); ++i) {
        INFO("1.1 {}", i);

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

double PR650::getLum() const { return lum_; }

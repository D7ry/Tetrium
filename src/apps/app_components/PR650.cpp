#include "PR650.h"
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#if defined(WIN32)
#include <windows.h>
#else
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/select.h>
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
    int attemptCount = 0;
    const int maxAttempts = 10;

    while (reply != "000\r\n" && attemptCount < maxAttempts) {
        attemptCount++;
        ERROR(
            "reply '{}' (size: {}), not 000, trying again (attempt {}/{})",
            reply,
            reply.size(),
            attemptCount,
            maxAttempts
        );
        if (!sendMessage("b1", reply, 2000)) { // Increased timeout to 2 seconds
            ERROR("sendMessage failed on attempt {}", attemptCount);
#if defined(WIN32)
            Sleep(1000); // Wait 1 second before retrying
#else
            usleep(1000000); // Wait 1 second before retrying
#endif
            continue;
        }
        INFO("Got reply: '{}' (size: {})", reply, reply.size());
    }

    if (reply != "000\r\n") {
        PANIC(
            "Failed to get '000\\r\\n' response from PR650 after {} attempts. Last reply: '{}'",
            maxAttempts,
            reply
        );
    }

    INFO("PR650 connected on {}, backlight on: {}", portName_, reply);
    // Sync mode 0 = free-run. exposureTimeMs >= 34 ensures >= one full 30 Hz cycle is
    // integrated, preventing aliasing regardless of when the exposure starts.
    std::ostringstream sCmd;
    sCmd << "s01,,," << std::setfill('0') << std::setw(4) << exposureTimeMs
         << "," << numExposures << ",200,05,0";
    ASSERT(sendMessage(sCmd.str(), reply, 1000));
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
    serialHandle_ = open(portName_.c_str(), O_RDWR | O_NOCTTY);
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
    // Use blocking read with timeout (similar to pyserial's readline behavior)
    // VMIN=1 means wait for at least 1 character
    // VTIME=0 means no inter-character timeout (we'll handle timeout in code)
    tty.c_cc[VMIN] = 1;  // Wait for at least 1 character
    tty.c_cc[VTIME] = 0; // No inter-character timeout

    if (tcsetattr(serialHandle_, TCSANOW, &tty) != 0) {
        perror("tcsetattr");
        return false;
    }

    // Set DTR and RTS signals (some devices need these to be active)
    int flags = TIOCM_DTR | TIOCM_RTS;
    if (ioctl(serialHandle_, TIOCMBIS, &flags) < 0) {
        WARN("Failed to set DTR/RTS signals (errno: {})", errno);
    } else {
        INFO("Set DTR and RTS signals");
    }

    // Wait for connection to stabilize (like Python version does)
    usleep(1000000); // 1 second delay
    INFO("Serial port opened and configured, waiting 1s for device to stabilize");
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
    // Flush any pending input (Windows equivalent of tcflush)
    PurgeComm(serialHandle_, PURGE_RXCLEAR);

    DWORD bytesWritten;
    std::string msg = message.back() == '\n' ? message : message + "\n";
    if (!WriteFile(serialHandle_, msg.c_str(), msg.length(), &bytesWritten, NULL)) {
        std::cerr << "Failed to write to port.\n";
        return false;
    }

    // Wait for data to be transmitted
    FlushFileBuffers(serialHandle_);
    Sleep(500); // Allow PR650 to process (500ms delay like Python version)

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
                free(buf);
                return false;
            }
        }

        if (bytesRead > 0) {
            INFO("Read {} bytes", bytesRead);
            totalData.insert(totalData.end(), buf, buf + bytesRead);

            // Check if we got a complete line (ends with \r\n or \n)
            if (totalData.size() >= 1 && totalData.back() == '\n') {
                break;
            }
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

        Sleep(10); // Small delay to avoid busy waiting
    }
    free(buf);

    if (totalData.empty()) {
        return false;
    }

    response = std::string(totalData.data(), totalData.size());
    INFO("response: {}", response);
#else
    // Flush any pending input
    tcflush(serialHandle_, TCIFLUSH);

    std::string msg = message.back() == '\n' ? message : message + "\n";
    INFO("Sending message: '{}' (length: {})", msg.substr(0, msg.length() - 1), msg.length());
    ssize_t bytesWritten = write(serialHandle_, msg.c_str(), msg.length());
    if (bytesWritten < 0) {
        perror("write");
        ERROR("Failed to write to serial port");
        return false;
    }
    if (bytesWritten != (ssize_t)msg.length()) {
        ERROR("Partial write: wrote {}/{} bytes", bytesWritten, msg.length());
        return false;
    }
    INFO("Wrote {} bytes", bytesWritten);

    tcdrain(serialHandle_); // Wait for all data to be transmitted
    usleep(500000);         // Allow PR650 to process (500ms delay like Python version)
    INFO("Waiting for response (timeout: {}ms)...", timeout);

    // Read until newline or timeout using select()
    auto start = std::chrono::steady_clock::now();
    std::vector<char> totalData;
    char buf[256];

    while (true) {
        // Use select() to wait for data to be available
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(serialHandle_, &readfds);

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - start
        )
                           .count();

        if (elapsed >= timeout) {
            if (totalData.empty()) {
                return false;
            }
            break; // Timeout, return what we have
        }

        int remaining_timeout = timeout - elapsed;
        if (remaining_timeout <= 0) {
            if (totalData.empty()) {
                ERROR(
                    "Timeout waiting for response (elapsed: {}ms, timeout: {}ms)", elapsed, timeout
                );
                return false;
            }
            break; // Timeout, return what we have
        }

        struct timeval tv;
        tv.tv_sec = remaining_timeout / 1000;
        tv.tv_usec = (remaining_timeout % 1000) * 1000;

        int select_result = select(serialHandle_ + 1, &readfds, nullptr, nullptr, &tv);

        if (select_result < 0) {
            if (errno == EINTR) {
                // Interrupted by signal, continue
                continue;
            }
            perror("select");
            ERROR("select() failed with errno: {}", errno);
            return false;
        } else if (select_result == 0) {
            // Timeout from select
            if (totalData.empty()) {
                ERROR(
                    "select() timed out after {}ms (total elapsed: {}ms) - no data received from "
                    "PR650",
                    remaining_timeout,
                    elapsed
                );
                // Try to read anyway in case there's a race condition
                int n = read(serialHandle_, buf, sizeof(buf) - 1);
                if (n > 0) {
                    INFO("Got data after select timeout: {} bytes", n);
                    buf[n] = '\0';
                    totalData.insert(totalData.end(), buf, buf + n);
                    if (totalData.back() == '\n') {
                        break;
                    }
                    continue;
                }
                return false;
            }
            break;
        }

        INFO("select() returned {} (data available)", select_result);

        // Data is available, read it
        if (FD_ISSET(serialHandle_, &readfds)) {
            int n = read(serialHandle_, buf, sizeof(buf) - 1);
            if (n > 0) {
                buf[n] = '\0';
                INFO("Read {} bytes: '{}'", n, std::string(buf, n));
                totalData.insert(totalData.end(), buf, buf + n);

                // Check if we got a complete line (ends with \n)
                if (totalData.size() >= 1 && totalData.back() == '\n') {
                    INFO("Got complete line (ends with \\n)");
                    break;
                }
            } else if (n < 0) {
                perror("read");
                ERROR("read() failed with errno: {}", errno);
                return false;
            } else {
                // EOF (n == 0)
                INFO("read() returned 0 (EOF)");
                break;
            }
        }
    }

    if (totalData.empty()) {
        ERROR("No data received from PR650");
        return false;
    }

    response = std::string(totalData.data(), totalData.size());
    INFO("Final response ({} bytes): '{}'", response.size(), response);
    // Log hex representation for debugging
    std::string hexRep;
    for (char c : response) {
        char hex[4];
        snprintf(hex, sizeof(hex), "%02X ", (unsigned char)c);
        hexRep += hex;
    }
    INFO("Response hex: {}", hexRep);
#endif
    return true;
}

bool PR650::sendMessageMultiLine(
    const std::string& message,
    std::vector<std::string>& responseLines,
    int timeout
)
{
    // For multi-line responses (like "d5"), we need to read all lines
    // until the device stops sending data
#if defined(WIN32)
    // Flush any pending input
    PurgeComm(serialHandle_, PURGE_RXCLEAR);

    DWORD bytesWritten;
    std::string msg = message.back() == '\n' ? message : message + "\n";
    if (!WriteFile(serialHandle_, msg.c_str(), msg.length(), &bytesWritten, NULL)) {
        std::cerr << "Failed to write to port.\n";
        return false;
    }

    FlushFileBuffers(serialHandle_);
    Sleep(500); // Allow PR650 to process

    char* buf = (char*)malloc(1024);
    DWORD bytesRead;
    std::vector<char> totalData;
    auto start = std::chrono::steady_clock::now();
    bool gotData = false;

    // Read all lines until timeout with no new data
    while (true) {
        if (!ReadFile(serialHandle_, buf, 1024, &bytesRead, NULL)) {
            DWORD error = GetLastError();
            if (error == ERROR_HANDLE_EOF) {
                break;
            } else if (error != ERROR_IO_PENDING) {
                free(buf);
                return false;
            }
        }

        if (bytesRead > 0) {
            gotData = true;
            totalData.insert(totalData.end(), buf, buf + bytesRead);
            // Reset timeout if we got data
            start = std::chrono::steady_clock::now();
        } else {
            // No data, check if we've timed out
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now() - start
            )
                               .count();
            if (elapsed > 1000 && gotData) { // 1 second of no data after getting some
                break;
            }
            if (elapsed > timeout) {
                break;
            }
        }
        Sleep(10);
    }
    free(buf);

    if (totalData.empty()) {
        return false;
    }

    std::string fullResponse(totalData.data(), totalData.size());
#else
    // Flush any pending input
    tcflush(serialHandle_, TCIFLUSH);

    std::string msg = message.back() == '\n' ? message : message + "\n";
    INFO("Sending multi-line message: '{}'", msg.substr(0, msg.length() - 1));
    ssize_t bytesWritten = write(serialHandle_, msg.c_str(), msg.length());
    if (bytesWritten < 0 || bytesWritten != (ssize_t)msg.length()) {
        ERROR("Failed to write to serial port");
        return false;
    }

    tcdrain(serialHandle_);
    usleep(500000); // Allow PR650 to process

    // Read all lines until timeout with no new data
    auto start = std::chrono::steady_clock::now();
    std::vector<char> totalData;
    char buf[256];
    bool gotData = false;
    const int noDataTimeout = 1000; // 1 second of no data after getting some

    while (true) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - start
        )
                           .count();

        // If we got data but haven't received any for 1 second, we're done
        if (gotData && elapsed > noDataTimeout) {
            INFO("No new data for {}ms, assuming complete", noDataTimeout);
            break;
        }
        // If total timeout exceeded, we're done
        if (elapsed > timeout) {
            if (!gotData) {
                ERROR("Timeout waiting for multi-line response");
                return false;
            }
            break;
        }

        // Use select to wait for data
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(serialHandle_, &readfds);

        int remaining_timeout
            = (timeout - elapsed < noDataTimeout) ? (timeout - elapsed) : noDataTimeout;
        struct timeval tv;
        tv.tv_sec = remaining_timeout / 1000;
        tv.tv_usec = (remaining_timeout % 1000) * 1000;

        int select_result = select(serialHandle_ + 1, &readfds, nullptr, nullptr, &tv);

        if (select_result < 0) {
            if (errno == EINTR)
                continue;
            perror("select");
            return false;
        } else if (select_result == 0) {
            // Timeout - if we got data, we're done; otherwise continue waiting
            if (gotData) {
                break;
            }
            continue;
        }

        // Data available, read it
        if (FD_ISSET(serialHandle_, &readfds)) {
            int n = read(serialHandle_, buf, sizeof(buf) - 1);
            if (n > 0) {
                gotData = true;
                buf[n] = '\0';
                totalData.insert(totalData.end(), buf, buf + n);
                start = std::chrono::steady_clock::now(); // Reset timeout
                INFO("Read {} bytes (total: {})", n, totalData.size());
            } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("read");
                return false;
            }
        }
    }

    if (totalData.empty()) {
        ERROR("No data received for multi-line response");
        return false;
    }

    std::string fullResponse(totalData.data(), totalData.size());
    INFO("Multi-line response received: {} bytes", fullResponse.size());
#endif

    // Split into lines
    std::istringstream iss(fullResponse);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() || responseLines.empty()) { // Keep empty lines if they're meaningful
            responseLines.push_back(line);
        }
    }

    INFO("Parsed {} lines from multi-line response", responseLines.size());
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

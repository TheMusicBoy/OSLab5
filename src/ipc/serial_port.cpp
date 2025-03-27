#include <ipc/serial_port.h>

#include <common/logging.h>
#include <common/exception.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <fcntl.h>
#include <termios.h>
#include <cstring>
#include <unistd.h>
#include <errno.h>
#endif

#include <set>

namespace NIpc {

////////////////////////////////////////////////////////////////////////////////

namespace {

////////////////////////////////////////////////////////////////////////////////

bool ReadToNl(char* buffer, std::string& data) {
    if (char* nl = strchr(buffer, '\n')) {
        *nl = '\0';
        data += buffer;
        size_t offset = nl - buffer + 1;
        buffer[256 - offset] = '\0';
        memmove(buffer, buffer + offset, 256 - offset);
        return false;
    } else {
        data += buffer;
        buffer[0] = '\0'; // Clear buffer after consuming
        return true;
    }
}

////////////////////////////////////////////////////////////////////////////////

} // namespace

////////////////////////////////////////////////////////////////////////////////

void TSerialConfig::Load(const nlohmann::json& data) {
    SerialPort = TConfigBase::LoadRequired<std::string>(data, "serial_port");
    BaudRate = TConfigBase::LoadRequired<unsigned>(data, "baud_rate");

    static const std::set<unsigned> kValidRates{9600, 19200, 38400, 57600, 115200};
    
    ASSERT(kValidRates.count(BaudRate), "Invalid baud rate: {}. (Valid rates: {})", BaudRate, NCommon::Join(kValidRates));
}

////////////////////////////////////////////////////////////////////////////////

TComPort::TComPort(TSerialConfigPtr config)
    : Config_(config)
{
    Open();
}

TComPort::~TComPort() {
    Close();
}

void TComPort::Open() {
    if (Connected_) {
        return;
    }

#if defined(_WIN32) || defined(_WIN64)
    Desc_ = CreateFileA(
        Config_->SerialPort.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    ASSERT(Desc_ != INVALID_HANDLE_VALUE, "Failed to open serial port: {}", GetLastError());

    if (!SetupPort()) {
        ClosePort();
        THROW("Failed to setup port");
    }

#else
    Desc_ = open(Config_->SerialPort.c_str(), O_RDWR | O_NOCTTY);
#endif
    if (Desc_ == -1) {
#if defined(_WIN32) || defined(_WIN64)
        LPSTR messageBuffer = nullptr;
        DWORD errorMessageId = GetLastError();
        size_t size = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            errorMessageId,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&messageBuffer,
            0,
            NULL);

        std::string message(messageBuffer, size);
        LocalFree(messageBuffer);
#else
        std::string message = strerror(errno);
#endif

        Close();
        THROW("Failed to open serial port: {}", message);
    }

    termios tty;
    if (tcgetattr(Desc_, &tty) != 0) {
#if defined(_WIN32) || defined(_WIN64)
        LPSTR messageBuffer = nullptr;
        DWORD errorMessageId = GetLastError();
        size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                     NULL, errorMessageId, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

        std::string message(messageBuffer, size);
        LocalFree(messageBuffer);
#else
        std::string message = strerror(errno);
#endif
        Close();
        THROW("tcgetattr ended with exception: {}", message);
    }

    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= CREAD | CLOCAL;
    tty.c_lflag &= ~ICANON;
    tty.c_lflag &= ~ECHO;
    tty.c_lflag &= ~ECHOE;
    tty.c_lflag &= ~ECHONL;
    tty.c_lflag &= ~ISIG;

    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

    tty.c_oflag &= ~OPOST;

    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 0;

    uint32_t speed;
    switch (Config_->BaudRate) {
        case 9600: speed = B9600; break;
        case 19200: speed = B19200; break;
        case 38400: speed = B38400; break;
        case 57600: speed = B57600; break;
        case 115200: speed = B115200; break;
        default: THROW("Unexpected boud rate: {}", Config_->BaudRate);
    }
    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    if (tcsetattr(Desc_, TCSANOW, &tty) != 0) {
#if defined(_WIN32) || defined(_WIN64)
        LPSTR messageBuffer = nullptr;
        DWORD errorMessageId = GetLastError();
        size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                     NULL, errorMessageId, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

        std::string message(messageBuffer, size);
        LocalFree(messageBuffer);
#else
        std::string message = strerror(errno);
#endif
        Close();
        THROW("tcsetattr ended with exception: {}", message);
    }

    Connected_ = true;
}

void TComPort::Close() {
    if (!Connected_) {
        return;
    }

#if defined(_WIN32) || defined(_WIN64)
    if (Desc_ != INVALID_HANDLE_VALUE) {
        CloseHandle(Desc_);
        Desc_ = INVALID_HANDLE_VALUE;
    }
#else
    if (Desc_ != -1) {
        close(Desc_);
        Desc_ = -1;
    }
#endif

    Connected_ = false;
}

size_t TComPort::Read(void* buffer, size_t size) {
#if defined(_WIN32) || defined(_WIN64)
    DWORD bytesRead;
    if (!ReadFile(Desc_, buffer, size, &bytesRead, NULL)) {
        return 0;
    }
    return bytesRead;
#else
    size_t bytesRead;
    bytesRead = std::max(read(Desc_, buffer, size), 0l);
    return bytesRead;
#endif
}

std::string TComPort::ReadLine() {
    if (!Connected_) {
        return "";
    }

    std::string data;
    while (ReadToNl(buffer, data)) {
        if (auto size = Read(buffer, sizeof(buffer) - 1)) {
            buffer[size] = '\0';
        } else {
            break;
        }
    }
    return data;
}

void TComPort::Write(const std::string& data) {
    if (!Connected_) {
        THROW("Port not open");
    }

#if defined(_WIN32) || defined(_WIN64)
    DWORD bytesWritten;
    BOOL success = WriteFile(Desc_, data.data(), static_cast<DWORD>(data.size()), &bytesWritten, NULL);
    if (!success || bytesWritten != static_cast<DWORD>(data.size())) {
        DWORD error = GetLastError();
        LPSTR messageBuffer = nullptr;
        size_t size = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            error,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&messageBuffer,
            0,
            NULL
        );
        std::string message(messageBuffer, size);
        LocalFree(messageBuffer);
        THROW("Write failed: {}", message);
    }
#else
    ssize_t bytesWritten = write(Desc_, data.data(), data.size());
    if (bytesWritten == -1 || bytesWritten != static_cast<ssize_t>(data.size())) {
        THROW("Write failed: {}", strerror(errno));
    }
#endif
}

bool TComPort::IsOpen() const {
    return Connected_;
}

#if defined(_WIN32) || defined(_WIN64)
bool TComPort::SetupPort() {
    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (!GetCommState(Desc_, &dcbSerialParams)) {

        LPSTR messageBuffer = nullptr;
        DWORD errorMessageId = GetLastError();
        size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                     NULL, errorMessageId, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

        std::string message(messageBuffer, size);
        LocalFree(messageBuffer);
        LOG_ERROR("Error getting port state: {}", message);
        return false;
    }

    uint32_t speed;
    switch (Config_->BaudRate) {
        case 9600: speed = CBR_9600; break;
        case 19200: speed = CBR_19200; break;
        case 38400: speed = CBR_38400; break;
        case 57600: speed = CBR_57600; break;
        case 115200: speed = CBR_115200; break;
        default: THROW("Unsupported boud rate: {}", Config_->BaudRate);
    }

    dcbSerialParams.BaudRate = speed;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;

    if (!SetCommState(Desc_, &dcbSerialParams)) {

        LPSTR messageBuffer = nullptr;
        DWORD errorMessageId = GetLastError();
        size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                     NULL, errorMessageId, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

        std::string message(messageBuffer, size);
        LocalFree(messageBuffer);
        LOG_ERROR("Error setting port state: {}", message);
        return false;
    }

    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    if (!SetCommTimeouts(Desc_, &timeouts)) {

        LPSTR messageBuffer = nullptr;
        DWORD errorMessageId = GetLastError();
        size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                     NULL, errorMessageId, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

        std::string message(messageBuffer, size);
        LocalFree(messageBuffer);
        LOG_ERROR("Error setting timeouts: {}", message);
        return false;
    }

    return true;
}
#endif

////////////////////////////////////////////////////////////////////////////////

} // namespace NIpc

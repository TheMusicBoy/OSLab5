#include <ipc/decode_encode.h>
#include <common/logging.h>
#include <common/exception.h>
#include <cstring>
#include <cmath>
#include <algorithm>


namespace NDecode {

namespace {

////////////////////////////////////////////////////////////////////////////////

inline const std::string LoggingSource = "TempDecoder";

constexpr uint8_t STX = 0x02;  // Start of Text
constexpr uint8_t ETX = 0x03;  // End of Text
constexpr uint8_t CR = 0x0D;   // Carriage Return
constexpr uint8_t LF = 0x0A;   // Line Feed

////////////////////////////////////////////////////////////////////////////////

} // namespace

////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<TTemperatureDecoderBase> CreateDecoder(NDecode::ETemperatureFormat format) {
    switch (format) {
        case NDecode::ETemperatureFormat::ByteInteger:
            return std::make_unique<TBinaryByteIntegerTemperatureDecoder>();
        case NDecode::ETemperatureFormat::FixedPoint:
            return std::make_unique<TBinaryFixedPointTemperatureDecoder>();
        case NDecode::ETemperatureFormat::FloatingPoint:
            return std::make_unique<TBinaryFloatingPointTemperatureDecoder>();
        case NDecode::ETemperatureFormat::Text:
        default:
            return std::make_unique<TTextTemperatureDecoder>();
    }
}

std::unique_ptr<TTemperatureEncoderBase> CreateEncoder(NDecode::ETemperatureFormat format) {
    switch (format) {
        case NDecode::ETemperatureFormat::ByteInteger:
            return std::make_unique<TBinaryByteIntegerTemperatureEncoderBase>();
        case NDecode::ETemperatureFormat::FixedPoint:
            return std::make_unique<TBinaryFixedPointTemperatureEncoderBase>();
        case NDecode::ETemperatureFormat::FloatingPoint:
            return std::make_unique<TBinaryFloatingPointTemperatureEncoderBase>();
        case NDecode::ETemperatureFormat::Text:
        default:
            return std::make_unique<TTextTemperatureEncoder>();
    }
}

////////////////////////////////////////////////////////////////////////////////

void TTemperatureDecoderBase::SetComPort(NIpc::TComPortPtr comPort) {
    ComPort_ = comPort;
    
    if (!ComPort_) {
        THROW("COM port pointer cannot be null");
    }
    
    if (!ComPort_->IsOpen()) {
        LOG_INFO("Opening COM port for temperature decoder");
        try {
            ComPort_->Open();
        } catch (const std::exception& ex) {
            LOG_ERROR("Failed to open COM port: {}", ex.what());
            throw;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

TTextTemperatureDecoder::TTextTemperatureDecoder()
    : Buffer_(256, 0)
{
}

double TTextTemperatureDecoder::ReadTemperature() {
    if (!ComPort_ || !ComPort_->IsOpen()) {
        THROW("Port not open or not initialized");
    }
    
    if (Buffer_.size() >= MaxBufferSize) {
        LOG_WARNING("Buffer exceeded maximum size ({}), resetting", MaxBufferSize);
        Buffer_.clear();
        Buffer_.resize(256, 0);
    }
    
    uint8_t tempBuffer[256] = {0};
    size_t bytesRead = ComPort_->Read(tempBuffer, sizeof(tempBuffer));
    
    if (bytesRead > 0) {
        LOG_DEBUG("Read {} bytes from serial port", bytesRead);
        
        size_t oldSize = Buffer_.size();
        Buffer_.resize(oldSize + bytesRead);
        std::memcpy(Buffer_.data() + oldSize, tempBuffer, bytesRead);
    }
    
    auto result = DecodeTextTemperature();
    
    ProcessBuffer();
    
    if (result) {
        LOG_DEBUG("Decoded temperature: {}", *result);
        return *result;
    }
    
    return NAN;
}

std::optional<double> TTextTemperatureDecoder::DecodeTextTemperature() {
    for (size_t pos = 0; pos < Buffer_.size() - 10; pos++) {
        if (Buffer_[pos] != STX) {
            continue;
        }
        
        if (pos + 2 >= Buffer_.size() || Buffer_[pos + 1] != 'T' || Buffer_[pos + 2] != '=') {
            continue;
        }
        
        size_t etxPos = pos + 3;
        while (etxPos < Buffer_.size() && Buffer_[etxPos] != ETX) {
            etxPos++;
        }
        
        if (etxPos >= Buffer_.size()) {
            break;
        }
        
        std::string tempStr;
        for (size_t i = pos + 3; i < etxPos; i++) {
            tempStr.push_back(static_cast<char>(Buffer_[i]));
        }
        
        if (tempStr.empty() || tempStr.back() != 'C') {
            LOG_WARNING("Invalid temperature format: {}", tempStr);
            continue;
        }
        
        tempStr.pop_back();
        
        try {
            double temp = std::stod(tempStr);
            return temp;
        } catch (const std::exception& ex) {
            LOG_WARNING("Failed to parse temperature: '{}', error: {}", tempStr, ex.what());
            continue;
        }
    }
    
    return std::nullopt;
}

size_t TTextTemperatureDecoder::ProcessBuffer() {
    for (size_t pos = 0; pos < Buffer_.size() - 5; pos++) {
        if (Buffer_[pos] != STX) {
            continue;
        }
        
        size_t etxPos = pos + 1;
        while (etxPos < Buffer_.size() && Buffer_[etxPos] != ETX) {
            etxPos++;
        }
        
        if (etxPos >= Buffer_.size()) {
            break;
        }
        
        if (etxPos + 2 < Buffer_.size() && Buffer_[etxPos + 1] == CR && Buffer_[etxPos + 2] == LF) {
            size_t msgEnd = etxPos + 3;
            
            if (msgEnd < Buffer_.size()) {
                std::copy(Buffer_.begin() + msgEnd, Buffer_.end(), Buffer_.begin());
                Buffer_.resize(Buffer_.size() - msgEnd);
                return msgEnd;
            } else {
                Buffer_.clear();
                Buffer_.resize(256, 0);
                return msgEnd;
            }
        }
    }
    
    return 0;
}
////////////////////////////////////////////////////////////////////////////////

TBinaryTemperatureDecoderBase::TBinaryTemperatureDecoderBase()
    : Buffer_(256, 0)
{ }

double TBinaryTemperatureDecoderBase::ReadTemperature() {
    if (!ComPort_ || !ComPort_->IsOpen()) {
        THROW("Port not open or not initialized");
    }
    
    if (Buffer_.size() >= MaxBufferSize) {
        LOG_WARNING("Buffer exceeded maximum size ({}), resetting", MaxBufferSize);
        Buffer_.clear();
        Buffer_.resize(256, 0);
    }
    
    uint8_t tempBuffer[256] = {0};
    size_t bytesRead = ComPort_->Read(tempBuffer, sizeof(tempBuffer));
    
    if (bytesRead > 0) {
        LOG_DEBUG("Read {} bytes from serial port", bytesRead);
        
        size_t oldSize = Buffer_.size();
        Buffer_.resize(oldSize + bytesRead);
        std::memcpy(Buffer_.data() + oldSize, tempBuffer, bytesRead);
    }
    
    auto result = DecodeBinaryTemperature(Buffer_.data(), Buffer_.size());
    
    ProcessBuffer();
    
    if (result) {
        LOG_DEBUG("Decoded temperature: {}", *result);
        return *result;
    }
    
    return NAN;
}

size_t TBinaryTemperatureDecoderBase::ProcessBuffer() {
    for (size_t pos = 0; pos < Buffer_.size() - 5; pos++) {
        if (Buffer_[pos] != 0x02) {
            continue;
        }
        
        if (Buffer_[pos + 1] != 0x54) {
            continue;
        }
        
        uint8_t dataLen = Buffer_[pos + 2];
        
        if (pos + 3 + dataLen + 2 > Buffer_.size()) {
            break;
        }
        
        if (Buffer_[pos + 3 + dataLen + 1] == 0x03) {
            size_t packetEnd = pos + 3 + dataLen + 2;
            
            if (packetEnd < Buffer_.size()) {
                std::copy(Buffer_.begin() + packetEnd, Buffer_.end(), Buffer_.begin());
                Buffer_.resize(Buffer_.size() - packetEnd);
                return packetEnd;
            } else {
                Buffer_.clear();
                Buffer_.resize(256, 0);
                return packetEnd;
            }
        }
    }
    
    return 0;
}

////////////////////////////////////////////////////////////////////////////////

std::optional<double> TBinaryByteIntegerTemperatureDecoder::DecodeBinaryTemperature(uint8_t* buffer, size_t size) {
    for (size_t pos = 0; pos < size - 5; pos++) {
        if (buffer[pos] != 0x02) {
            continue;
        }
        
        uint8_t cmd = buffer[pos + 1];
        if (cmd != 0x54) {
            continue;
        }
        
        uint8_t dataLen = buffer[pos + 2];
        if (dataLen != 1) {
            continue;
        }
        
        if (pos + 3 + dataLen + 2 > size) {
            break;
        }
        
        if (buffer[pos + 3 + dataLen + 1] != 0x03) {
            continue;
        }
        
        uint8_t calculatedChecksum = cmd ^ dataLen ^ buffer[pos + 3];
        
        uint8_t packetChecksum = buffer[pos + 3 + dataLen];
        if (calculatedChecksum != packetChecksum) {
            LOG_WARNING("Checksum mismatch: calculated={:#x}, received={:#x}", 
                      calculatedChecksum, packetChecksum);
            continue;
        }
        
        const uint8_t* data = buffer + pos + 3;
        return static_cast<double>(static_cast<int8_t>(data[0]));
    }
    
    return std::nullopt;
}

////////////////////////////////////////////////////////////////////////////////

std::optional<double> TBinaryFixedPointTemperatureDecoder::DecodeBinaryTemperature(uint8_t* buffer, size_t size) {
    for (size_t pos = 0; pos < size - 6; pos++) {
        if (buffer[pos] != 0x02) {
            continue;
        }
        
        uint8_t cmd = buffer[pos + 1];
        if (cmd != 0x54) {
            continue;
        }
        
        uint8_t dataLen = buffer[pos + 2];
        if (dataLen != 2) {
            continue;
        }
        
        if (pos + 3 + dataLen + 2 > size) {
            break;
        }
        
        if (buffer[pos + 3 + dataLen + 1] != 0x03) {
            continue;
        }
        
        uint8_t calculatedChecksum = cmd ^ dataLen;
        for (size_t i = 0; i < dataLen; i++) {
            calculatedChecksum ^= buffer[pos + 3 + i];
        }
        
        uint8_t packetChecksum = buffer[pos + 3 + dataLen];
        if (calculatedChecksum != packetChecksum) {
            LOG_WARNING("Checksum mismatch: calculated={:#x}, received={:#x}", 
                      calculatedChecksum, packetChecksum);
            continue;
        }
        
        const uint8_t* data = buffer + pos + 3;
        uint16_t tempInt = (data[0] << 8) | data[1];
        return tempInt / 10.0;
    }
    
    return std::nullopt;
}

////////////////////////////////////////////////////////////////////////////////

std::optional<double> TBinaryFloatingPointTemperatureDecoder::DecodeBinaryTemperature(uint8_t* buffer, size_t size) {
    for (size_t pos = 0; pos < size - 8; pos++) {
        if (buffer[pos] != 0x02) {
            continue;
        }
        
        uint8_t cmd = buffer[pos + 1];
        if (cmd != 0x54) {
            continue;
        }
        
        uint8_t dataLen = buffer[pos + 2];
        if (dataLen != 4) {
            continue;
        }
        
        if (pos + 3 + dataLen + 2 > size) {
            break;
        }
        
        if (buffer[pos + 3 + dataLen + 1] != 0x03) {
            continue;
        }
        
        uint8_t calculatedChecksum = cmd ^ dataLen;
        for (size_t i = 0; i < dataLen; i++) {
            calculatedChecksum ^= buffer[pos + 3 + i];
        }
        
        uint8_t packetChecksum = buffer[pos + 3 + dataLen];
        if (calculatedChecksum != packetChecksum) {
            LOG_WARNING("Checksum mismatch: calculated={:#x}, received={:#x}", 
                      calculatedChecksum, packetChecksum);
            continue;
        }
        
        const uint8_t* data = buffer + pos + 3;
        float tempValue;
        uint32_t tempBits = (data[0] << 24) | (data[1] << 16) | 
                           (data[2] << 8) | data[3];
        std::memcpy(&tempValue, &tempBits, sizeof(float));
        return static_cast<double>(tempValue);
    }
    
    return std::nullopt;
}

////////////////////////////////////////////////////////////////////////////////

void TTemperatureEncoderBase::SetComPort(NIpc::TComPortPtr comPort) {
    ComPort_ = comPort;
    
    if (!ComPort_) {
        THROW("COM port pointer cannot be null");
    }
    
    if (!ComPort_->IsOpen()) {
        LOG_INFO("Opening COM port for temperature encoder");
        try {
            ComPort_->Open();
        } catch (const std::exception& ex) {
            LOG_ERROR("Failed to open COM port: {}", ex.what());
            throw;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

void TTextTemperatureEncoder::WriteTemperature(double value) {
    if (!ComPort_ || !ComPort_->IsOpen()) {
        THROW("Port not open or not initialized");
    }
    
    std::ostringstream oss;
    
    std::string tempStr;
    if (value >= 0) {
        tempStr = "+";
    }
    
    oss << std::fixed << std::setprecision(Precision_) << value;
    tempStr += oss.str() + "C";
    
    std::vector<uint8_t> data;
    data.push_back(STX);            // Start of Text
    data.push_back('T');            // "T"
    data.push_back('=');            // "="
    
    for (char c : tempStr) {
        data.push_back(static_cast<uint8_t>(c));
    }
    
    data.push_back(ETX);            // End of Text
    data.push_back(CR);             // Carriage Return
    data.push_back(LF);             // Line Feed
    
    std::string packetStr(reinterpret_cast<char*>(data.data()), data.size());
    
    ComPort_->Write(packetStr);
    
    LOG_DEBUG("Temperature sent as text: {}", tempStr);
}

void TTextTemperatureEncoder::SetPrecision(int precision) {
    Precision_ = precision;
}

////////////////////////////////////////////////////////////////////////////////

void TBinaryTemperatureEncoderBase::WriteTemperature(double value) {
    if (!ComPort_ || !ComPort_->IsOpen()) {
        THROW("Port not open or not initialized");
    }
    
    std::vector<uint8_t> data = EncodeBinaryTemperature(value);
    
    std::vector<uint8_t> packet;
    
    packet.push_back(0x02);
    
    packet.push_back(0x54);
    
    packet.push_back(static_cast<uint8_t>(data.size()));
    
    packet.insert(packet.end(), data.begin(), data.end());
    
    uint8_t checksum = 0x54 ^ static_cast<uint8_t>(data.size());
    for (auto b : data) {
        checksum ^= b;
    }
    packet.push_back(checksum);
    
    packet.push_back(0x03);
    
    std::string packetStr(reinterpret_cast<char*>(packet.data()), packet.size());
    
    ComPort_->Write(packetStr);
    
    std::string hexData;
    for (auto b : packet) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%02X ", b);
        hexData += buf;
    }
    
    LOG_DEBUG("Temperature sent: {}, Packet: {}", value, hexData);
}

////////////////////////////////////////////////////////////////////////////////

std::vector<uint8_t> TBinaryByteIntegerTemperatureEncoderBase::EncodeBinaryTemperature(double value) {
    std::vector<uint8_t> data;
    
    if (value < -128.0 || value > 127.0) {
        LOG_WARNING("Temperature value {} outside int8_t range (-128 to 127), clamping", value);
        value = std::clamp(value, -128.0, 127.0);
    }

    int8_t tempInt = static_cast<int8_t>(std::round(value));
    data.push_back(static_cast<uint8_t>(tempInt));
    
    return data;
}

std::vector<uint8_t> TBinaryFixedPointTemperatureEncoderBase::EncodeBinaryTemperature(double value) {
    std::vector<uint8_t> data;
    
    int16_t tempFixed = static_cast<int16_t>(std::round(value * 10));
    
    data.push_back(static_cast<uint8_t>((tempFixed >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>(tempFixed & 0xFF));
    
    return data;
}

std::vector<uint8_t> TBinaryFloatingPointTemperatureEncoderBase::EncodeBinaryTemperature(double value) {
    std::vector<uint8_t> data;
    
    float tempFloat = static_cast<float>(value);
    uint32_t tempBits;
    std::memcpy(&tempBits, &tempFloat, sizeof(float));
    
    data.push_back(static_cast<uint8_t>((tempBits >> 24) & 0xFF));
    data.push_back(static_cast<uint8_t>((tempBits >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((tempBits >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>(tempBits & 0xFF));
    
    return data;
}

////////////////////////////////////////////////////////////////////////////////

} // namespace NDecode

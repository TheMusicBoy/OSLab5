#include <ipc/serial_port.h>

namespace NDecode {

////////////////////////////////////////////////////////////////////////////////

enum class ETemperatureFormat {
    Text,           // Plain text (default)
    ByteInteger,    // 1-byte integer binary format
    FixedPoint,     // 2-byte fixed-point binary format (0.1 precision)
    FloatingPoint   // 4-byte IEEE754 floating point binary format
};

inline ETemperatureFormat ParseTemperatureFormat(const std::string& format) {
    if (format == "byte_integer") return ETemperatureFormat::ByteInteger;
    if (format == "fixed_point") return ETemperatureFormat::FixedPoint;
    if (format == "floating_point") return ETemperatureFormat::FloatingPoint;
    return ETemperatureFormat::Text;
}

////////////////////////////////////////////////////////////////////////////////

class TTemperatureDecoderBase {
public:
    virtual double ReadTemperature() = 0;

    void SetComPort(NIpc::TComPortPtr comPort);

protected: NIpc::TComPortPtr ComPort_; };

std::unique_ptr<TTemperatureDecoderBase> CreateDecoder(ETemperatureFormat format);

////////////////////////////////////////////////////////////////////////////////

class TTextTemperatureDecoder
    : public TTemperatureDecoderBase
{
public:
    TTextTemperatureDecoder();
    double ReadTemperature() override;

private:
    std::optional<double> DecodeTextTemperature();
    size_t ProcessBuffer();
    
    std::vector<uint8_t> Buffer_;
    static constexpr size_t MaxBufferSize = 1024;
};

////////////////////////////////////////////////////////////////////////////////

class TBinaryTemperatureDecoderBase
    : public TTemperatureDecoderBase
{
public:
    TBinaryTemperatureDecoderBase();
    double ReadTemperature() override;

protected:
    virtual std::optional<double> DecodeBinaryTemperature(uint8_t* buffer, size_t size) = 0;
    
    std::vector<uint8_t> Buffer_;
    static constexpr size_t MaxBufferSize = 1024;
    
    size_t ProcessBuffer();
};

////////////////////////////////////////////////////////////////////////////////

class TBinaryByteIntegerTemperatureDecoder
    : public TBinaryTemperatureDecoderBase
{
protected:
    std::optional<double> DecodeBinaryTemperature(uint8_t* buffer, size_t size) override;

};

class TBinaryFixedPointTemperatureDecoder
    : public TBinaryTemperatureDecoderBase
{
protected:
    std::optional<double> DecodeBinaryTemperature(uint8_t* buffer, size_t size) override;

};

class TBinaryFloatingPointTemperatureDecoder
    : public TBinaryTemperatureDecoderBase
{
protected:
    std::optional<double> DecodeBinaryTemperature(uint8_t* buffer, size_t size) override;

};

////////////////////////////////////////////////////////////////////////////////

class TTemperatureEncoderBase {
public:
    virtual void WriteTemperature(double value) = 0;

    void SetComPort(NIpc::TComPortPtr comPort);

protected:
    NIpc::TComPortPtr ComPort_;

};

std::unique_ptr<TTemperatureEncoderBase> CreateEncoder(ETemperatureFormat format);

////////////////////////////////////////////////////////////////////////////////

class TTextTemperatureEncoder
    : public TTemperatureEncoderBase
{
public:
    TTextTemperatureEncoder() = default;
    void WriteTemperature(double value) override;
    
    void SetPrecision(int precision);
    
private:
    int Precision_ = 1;
};

////////////////////////////////////////////////////////////////////////////////

class TBinaryTemperatureEncoderBase
    : public TTemperatureEncoderBase
{
public:
    void WriteTemperature(double value) override;

protected:
    virtual std::vector<uint8_t> EncodeBinaryTemperature(double value) = 0;

};

////////////////////////////////////////////////////////////////////////////////

class TBinaryByteIntegerTemperatureEncoderBase
    : public TBinaryTemperatureEncoderBase
{
protected:
    std::vector<uint8_t> EncodeBinaryTemperature(double value) override;
};

class TBinaryFixedPointTemperatureEncoderBase
    : public TBinaryTemperatureEncoderBase
{
protected:
    std::vector<uint8_t> EncodeBinaryTemperature(double value) override;
};

class TBinaryFloatingPointTemperatureEncoderBase
    : public TBinaryTemperatureEncoderBase
{
protected:
    std::vector<uint8_t> EncodeBinaryTemperature(double value) override;
};

////////////////////////////////////////////////////////////////////////////////

} // namespace NDecode

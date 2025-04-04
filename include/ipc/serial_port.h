#pragma once

#include <common/intrusive_ptr.h>
#include <common/config.h>

#include <string>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

namespace NIpc {

////////////////////////////////////////////////////////////////////////////////

struct TSerialConfig
    : public NCommon::TConfigBase
{
    std::string SerialPort;
    unsigned BaudRate;
    std::string Format;

    void Load(const nlohmann::json& data) override;
};

DECLARE_REFCOUNTED(TSerialConfig);

////////////////////////////////////////////////////////////////////////////////

class TComPort
    : public NRefCounted::TRefCountedBase
{
public:
    TComPort(TSerialConfigPtr config);
    ~TComPort();

    void Open();
    void Close();
    size_t Read(void* buffer, size_t size);
    void Write(const std::string& data);

    bool IsOpen() const;

private:
#if defined(_WIN32) || defined(_WIN64)
    bool setupPort();
#endif

#if defined(_WIN32) || defined(_WIN64)
    HANDLE Desc_ = NULL;
#else
    int Desc_ = -1;
#endif

    TSerialConfigPtr Config_;

    bool Connected_ = false;
};

DECLARE_REFCOUNTED(TComPort);

////////////////////////////////////////////////////////////////////////////////

} // namespace NIpc

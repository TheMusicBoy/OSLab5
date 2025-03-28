#pragma once

#include <ipc/serial_port.h>
#include <ipc/db_client.h>

#include <common/config.h>
#include <common/refcounted.h>
#include <common/intrusive_ptr.h>

#include <nlohmann/json.hpp>

#include <filesystem>

namespace NConfig {

////////////////////////////////////////////////////////////////////////////////

struct TSimulatorConfig
    : public NCommon::TConfigBase
{
    double TimeMultiplier = 1.0;
    NIpc::TSerialConfigPtr SerialConfig;

    void Load(const nlohmann::json& data) override;
};

DECLARE_REFCOUNTED(TSimulatorConfig);

////////////////////////////////////////////////////////////////////////////////

struct TFileStorageConfig
    : public NCommon::TConfigBase
{
    std::filesystem::path TemperaturePath;
    std::filesystem::path TemperatureHourPath;
    std::filesystem::path TemperatureDayPath;

    void Load(const nlohmann::json& data) override;
};

DECLARE_REFCOUNTED(TFileStorageConfig);

////////////////////////////////////////////////////////////////////////////////

struct TStorageConfig
    : public NCommon::TConfigBase
{
    TFileStorageConfigPtr FileStorageConfig;
    NIpc::TDataBaseConfigPtr DataBaseConfig;
    
    void Load(const nlohmann::json& data) override;
};

DECLARE_REFCOUNTED(TStorageConfig);

////////////////////////////////////////////////////////////////////////////////

struct TConfig
    : public NCommon::TConfigBase
{
    unsigned MesureDelay = 100;

    std::filesystem::path AssetsPath = "/home/painfire/assets";

    NIpc::TSerialConfigPtr SerialConfig;
    TStorageConfigPtr StorageConfig;

    void Load(const nlohmann::json& data) override;
};

DECLARE_REFCOUNTED(TConfig);

////////////////////////////////////////////////////////////////////////////////

} // namespace NConfig

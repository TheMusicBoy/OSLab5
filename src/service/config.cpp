#include "config.h"

#include <common/logging.h>

namespace NConfig {

////////////////////////////////////////////////////////////////////////////////

void TSimulatorConfig::Load(const nlohmann::json& data) {
    SerialConfig = TConfigBase::LoadRequired<NIpc::TSerialConfig>(data, "serial");
    TimeMultiplier = TConfigBase::Load<double>(data, "time_multiplier", 1);
    DelayMs = TConfigBase::Load<uint32_t>(data, "delay_ms", 100);
}

////////////////////////////////////////////////////////////////////////////////

void TFileStorageConfig::Load(const nlohmann::json& data) {
    TemperaturePath = TConfigBase::LoadRequired<std::string>(data, "temperature");
    TemperatureHourPath = TConfigBase::LoadRequired<std::string>(data, "hourly");
    TemperatureDayPath = TConfigBase::LoadRequired<std::string>(data, "daily");
}

////////////////////////////////////////////////////////////////////////////////

void TStorageConfig::Load(const nlohmann::json& data) {
    ASSERT(!data.contains("file_system") || !data.contains("db_client"), "Config must contain only one system of storage data");
    ASSERT(data.contains("file_system") || data.contains("db_client"), "Config must contain file_system or db_client config");

    if (data.contains("file_system")) {
        FileStorageConfig = TConfigBase::LoadRequired<NConfig::TFileStorageConfig>(data, "file_system");
    } else if (data.contains("db_client")) {
        DataBaseConfig = TConfigBase::LoadRequired<NIpc::TDataBaseConfig>(data, "db_client");
    }
}

////////////////////////////////////////////////////////////////////////////////

void TLogDestinationConfig::Load(const nlohmann::json& data) {
    Path = TConfigBase::LoadRequired<std::string>(data, "path");
    
    std::string levelStr = TConfigBase::Load<std::string>(data, "level", "Info");
    if (levelStr == "Debug") Level = NLogging::ELevel::Debug;
    else if (levelStr == "Info") Level = NLogging::ELevel::Info;
    else if (levelStr == "Warning") Level = NLogging::ELevel::Warning;
    else if (levelStr == "Error") Level = NLogging::ELevel::Error;
    else if (levelStr == "Fatal") Level = NLogging::ELevel::Fatal;
}

////////////////////////////////////////////////////////////////////////////////

void TConfig::Load(const nlohmann::json& data) {
    MesureDelay = TConfigBase::Load<unsigned>(data, "mesure_delay", 100);
    Port = TConfigBase::Load<uint32_t>(data, "port", 8080);

    if (data.contains("logging") && data["logging"].is_array()) {
        for (const auto& dest : data["logging"]) {
            auto logConfig = NCommon::New<TLogDestinationConfig>();
            logConfig->Load(dest);
            LogDestinations.push_back(logConfig);
        }
    }

    SerialConfig = TConfigBase::LoadRequired<NIpc::TSerialConfig>(data, "serial");
    StorageConfig = TConfigBase::LoadRequired<TStorageConfig>(data, "storage");
}

} // namespace NConfig

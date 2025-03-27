#include <common/logging.h>

#include <config.h>

namespace NConfig {

////////////////////////////////////////////////////////////////////////////////

void TSimulatorConfig::Load(const nlohmann::json& data) {
    SerialConfig = TConfigBase::LoadRequired<NIpc::TSerialConfig>(data, "serial");
    TimeMultiplier = TConfigBase::Load<double>(data, "time_multiplier", TimeMultiplier);
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

void TConfig::Load(const nlohmann::json& data) {
    MesureDelay = TConfigBase::Load<unsigned>(data, "mesure_delay", MesureDelay);

    AssetsPath = TConfigBase::Load<std::string>(data, "assets_path", "/home/painfire/assets");

    SerialConfig = TConfigBase::LoadRequired<NIpc::TSerialConfig>(data, "serial");
    StorageConfig = TConfigBase::LoadRequired<TStorageConfig>(data, "storage");
}

} // namespace NConfig

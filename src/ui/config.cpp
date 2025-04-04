#include "config.h"

#include <common/logging.h>

namespace NConfig {

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
    ServiceEndpoint = TConfigBase::Load<std::string>(data, "service_endpoint", "http://localhost:8081");
    Port = TConfigBase::Load<uint32_t>(data, "port", 8080);
    AssetsPath = TConfigBase::Load<std::string>(data, "assets_path", "/home/painfire/assets");
    if (data.contains("logging") && data["logging"].is_array()) {
        for (const auto& dest : data["logging"]) {
            auto logConfig = NCommon::New<TLogDestinationConfig>();
            logConfig->Load(dest);
            LogDestinations.push_back(logConfig);
        }
    }
}

} // namespace NConfig

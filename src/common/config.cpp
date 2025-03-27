#include <common/config.h>

namespace NCommon {

////////////////////////////////////////////////////////////////////////////////

void TConfigBase::LoadFromFile(const std::filesystem::path& filePath) {
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            THROW("Failed to open config file: {}", filePath);
        }

        nlohmann::json configJson;
        file >> configJson;

        return this->Load(configJson);
    } catch (const nlohmann::json::exception& e) {
        LOG_ERROR("JSON parsing error: {}", e.what());
        RETHROW(e, "Invalid config file format");
    } catch (const std::exception& e) {
        RETHROW(e, "Config loading failed");
    }
}


////////////////////////////////////////////////////////////////////////////////

} // namespace NCommon 

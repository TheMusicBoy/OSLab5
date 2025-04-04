#pragma once

#include <common/config.h>
#include <common/refcounted.h>
#include <common/intrusive_ptr.h>

#include <filesystem>

namespace NConfig {

////////////////////////////////////////////////////////////////////////////////

struct TLogDestinationConfig
    : public NCommon::TConfigBase
{
    NLogging::ELevel Level = NLogging::ELevel::Info;
    std::string Path;

    void Load(const nlohmann::json& data) override;
};

DECLARE_REFCOUNTED(TLogDestinationConfig);
struct TConfig
    : public NCommon::TConfigBase
{
    std::filesystem::path AssetsPath;
    std::vector<TLogDestinationConfigPtr> LogDestinations;
    std::string ServiceEndpoint;
    uint32_t Port;

    void Load(const nlohmann::json& data) override;
};

DECLARE_REFCOUNTED(TConfig);

////////////////////////////////////////////////////////////////////////////////

} // namespace NConfig

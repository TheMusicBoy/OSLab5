#include <ui/service.h>

#include <common/logging.h>
#include <common/periodic_executor.h>
#include <common/weak_ptr.h>
#include <common/threadpool.h>
#include <ipc/serial_port.h>

#include <chrono>
#include <iomanip>
#include <ctime>


namespace NService {

////////////////////////////////////////////////////////////////////////////////

namespace {

////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
time_t timegm(struct tm* tm) {
    _tzset();
    return _mkgmtime(tm);
}
#endif

////////////////////////////////////////////////////////////////////////////////

} // namespace

////////////////////////////////////////////////////////////////////////////////

TService::TService(NConfig::TConfigPtr config)
    : Config_(std::move(config)),
      Assets_(NCommon::New<NAssets::TAssetsManager>(Config_->AssetsPath))
{
    Assets_->PreloadAssets();
}

TService::~TService() = default;

////////////////////////////////////////////////////////////////////////////////

NRpc::TResponse TService::HandleRawReadings(const NRpc::TRequest& request) {
    if (IsAcceptType(request, "text/html")) {
        auto asset = Assets_->LoadAsset("readings_list.html");

        if (asset.Format({{"status", "ok"}, {"period", "Raw Data"}, {"service_endpoint", Config_->ServiceEndpoint}})) {
            return NRpc::TResponse()
                .SetStatus(NRpc::EHttpCode::Ok)
                .SetAsset(asset);
        } else {
            return NRpc::TResponse().SetStatus(NRpc::EHttpCode::InternalError);
        }
    }

    return NRpc::TResponse().SetStatus(NRpc::EHttpCode::BadRequest);
}

NRpc::TResponse TService::HandleHourlyAverages(const NRpc::TRequest& request) {
    if (IsAcceptType(request, "text/html")) {
        auto asset = Assets_->LoadAsset("readings_list.html");

        if (asset.Format({{"status", "ok"}, {"period", "Hourly Averages"}, {"service_endpoint", Config_->ServiceEndpoint}})) {
            return NRpc::TResponse()
            .SetStatus(NRpc::EHttpCode::Ok)
            .SetAsset(asset);
        } else {
            return NRpc::TResponse().SetStatus(NRpc::EHttpCode::InternalError);
        }
    }

    return NRpc::TResponse().SetStatus(NRpc::EHttpCode::BadRequest);
}

NRpc::TResponse TService::HandleDailyAverages(const NRpc::TRequest& request) {
    if (IsAcceptType(request, "text/html")) {
        auto asset = Assets_->LoadAsset("readings_list.html");
        
        if (asset.Format({{"status", "ok"}, {"period", "Daily Averages"}, {"service_endpoint", Config_->ServiceEndpoint}})) {
            return NRpc::TResponse()
                .SetStatus(NRpc::EHttpCode::Ok)
                .SetAsset(asset);
        } else {
            return NRpc::TResponse().SetStatus(NRpc::EHttpCode::InternalError);
        }
    }

    return NRpc::TResponse().SetStatus(NRpc::EHttpCode::BadRequest);
}

NRpc::TResponse TService::HandleMainPage(const NRpc::TRequest& request) {
    if (IsAcceptType(request, "text/html")) {
        auto asset = Assets_->LoadAsset("main.html");
        return NRpc::TResponse()
            .SetStatus(NRpc::EHttpCode::Ok)
            .SetAsset(asset);
    }

    return NRpc::TResponse().SetStatus(NRpc::EHttpCode::BadRequest);
}

NRpc::TResponse TService::HandleNotFoundPage(const NRpc::TRequest& request) {
    if (IsAcceptType(request, "text/html")) {
        auto asset = Assets_->LoadAsset("not_found.html");
        return NRpc::TResponse()
            .SetStatus(NRpc::EHttpCode::NotFound)
            .SetAsset(asset);
    }

    return NRpc::TResponse().SetStatus(NRpc::EHttpCode::BadRequest);
}

NRpc::TResponse TService::HandleAssets(const NRpc::TRequest& request) {
    std::filesystem::path file = request.GetURL().substr(sizeof("/assets"));
    return Assets_->HandleRequest(file);
}

////////////////////////////////////////////////////////////////////////////////

} // namespace NService

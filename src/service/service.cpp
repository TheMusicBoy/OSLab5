#include <service/service.h>

#include <service/file_storage.h>
#include <service/database_storage.h>

#include <common/logging.h>
#include <common/periodic_executor.h>
#include <common/weak_ptr.h>
#include <common/threadpool.h>
#include <ipc/serial_port.h>

#include <chrono>
#include <deque>
#include <mutex>
#include <limits>
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

jinja2::ValuesMap ConvertReadingToJinja(const TReading& reading) {
    auto time = std::chrono::system_clock::to_time_t(reading.timestamp);
    std::ostringstream timestampStream;
    timestampStream << std::put_time(std::gmtime(&time), "%Y-%m-%d %H:%M:%S");
    
    return {
        {"timestamp", timestampStream.str()},
        {"temperature", reading.temperature}
    };
}

nlohmann::json CreateReadingsToJson(const std::deque<TReading>& readings, const std::string& period) {
    nlohmann::json response;
    response["status"] = "ok";
    response["period"] = period;
    
    nlohmann::json readingsArray = nlohmann::json::array();
    for (const auto& reading : readings) {
        auto time = std::chrono::system_clock::to_time_t(reading.timestamp);
        std::ostringstream timestampStream;
        timestampStream << std::put_time(std::gmtime(&time), "%Y-%m-%d %H:%M:%S");
        
        nlohmann::json item;
        item["timestamp"] = timestampStream.str();
        item["temperature"] = reading.temperature;
        readingsArray.push_back(item);
    }
    
    response["readings"] = readingsArray;
    response["count"] = readings.size();
    return response;
}

////////////////////////////////////////////////////////////////////////////////

} // namespace

////////////////////////////////////////////////////////////////////////////////

TService::TService(NConfig::TConfigPtr config, std::function<std::optional<TReading>(double)> processor)
    : Config_(std::move(config)),
      Port_(NCommon::New<NIpc::TComPort>(Config_->SerialConfig)),
      Assets_(NCommon::New<NAssets::TAssetsManager>(Config_->AssetsPath)),
      Processor_(processor),
      ThreadPool_(NCommon::New<NCommon::TThreadPool>(2)),
      Invoker_(NCommon::New<NCommon::TInvoker>(ThreadPool_))
{
    Assets_->PreloadAssets();
    if (Config_->StorageConfig->DataBaseConfig) {
        Storage_ = std::make_unique<TDataBaseStorage>(Config_->StorageConfig->DataBaseConfig);
    } else if (Config_->StorageConfig->FileStorageConfig) {
        Storage_ = std::make_unique<TFileStorage>(Config_->StorageConfig->FileStorageConfig);
    } else {
        THROW("Something went wrong, no storage configured.");
    }
}

TService::~TService() {
    MesurePeriodicExecutor_->Stop();
}

void TService::Start() {
    LOG_INFO("Starting service with measurement interval {} milliseconds", 
            Config_->MesureDelay);

    MesurePeriodicExecutor_ = NCommon::New<NCommon::TPeriodicExecutor>(
        NCommon::Bind(&TService::MesureTemperature, MakeWeak(this)),
        Invoker_,
        duration_cast<std::chrono::milliseconds>(std::chrono::milliseconds(Config_->MesureDelay))
    );

    MesurePeriodicExecutor_->Start();
}

void TService::MesureTemperature() {
    try {
        std::string response = Port_->ReadLine();
        
        if (response.empty()) {
            return;
        }

        if (auto reading = Processor_(std::stod(response))) {
            Invoker_->Run(NCommon::Bind(
                &TService::ProcessTemperature,
                MakeWeak(this),
                *reading
            ));
        }
    } catch (const NCommon::TException& ex) {
        LOG_ERROR("Temperature measurement failed: {}", ex.what());
    }
}

void TService::ProcessTemperature(TReading reading) {
    Storage_->ProcessTemperature(reading);
}

////////////////////////////////////////////////////////////////////////////////

NRpc::TResponse TService::HandleRawReadings(const NRpc::TRequest& request) {
    auto readingList = Storage_->GetRawReadings();
    
    if (IsAcceptType(request, "text/html")) {
        auto asset = Assets_->LoadAsset("readings_list.html");
        
        jinja2::ValuesList readings;
        for(const auto& reading : readingList) {
            readings.push_back(ConvertReadingToJinja(reading));
        }

        if (asset.Format({{"status", "ok"}, {"period", "Raw Data"}, {"readings", readings}})) {
            return NRpc::TResponse()
                .SetStatus(NRpc::EHttpCode::Ok)
                .SetAsset(asset);
        } else {
            return NRpc::TResponse().SetStatus(NRpc::EHttpCode::InternalError);
        }
    }

    if (IsAcceptType(request, "application/json")) {
        nlohmann::json jsonResponse = CreateReadingsToJson(readingList, "Raw Data");
        return NRpc::TResponse()
            .SetStatus(NRpc::EHttpCode::Ok)
            .SetJson(jsonResponse);
    }

    return NRpc::TResponse().SetStatus(NRpc::EHttpCode::BadRequest);
}

NRpc::TResponse TService::HandleHourlyAverages(const NRpc::TRequest& request) {
    auto readingList = Storage_->GetHourlyAverage();
    
    if (IsAcceptType(request, "text/html")) {
        auto asset = Assets_->LoadAsset("readings_list.html");

        jinja2::ValuesList readings;
        for(const auto& reading : readingList) {
            readings.push_back(ConvertReadingToJinja(reading));
        }

        if (asset.Format({{"status", "ok"}, {"period", "Hourly Averages"}, {"readings", readings}})) {
            return NRpc::TResponse()
            .SetStatus(NRpc::EHttpCode::Ok)
            .SetAsset(asset);
        } else {
            return NRpc::TResponse().SetStatus(NRpc::EHttpCode::InternalError);
        }
    }

    if (IsAcceptType(request, "application/json")) {
        nlohmann::json jsonResponse = CreateReadingsToJson(readingList, "Hourly Averages");
        return NRpc::TResponse()
            .SetStatus(NRpc::EHttpCode::Ok)
            .SetJson(jsonResponse);
    }
    
    return NRpc::TResponse().SetStatus(NRpc::EHttpCode::BadRequest);
}

NRpc::TResponse TService::HandleDailyAverages(const NRpc::TRequest& request) {
    auto readingList = Storage_->GetDailyAverage();
    
    if (IsAcceptType(request, "text/html")) {
        auto asset = Assets_->LoadAsset("readings_list.html");
        
        jinja2::ValuesList readings;
        for(const auto& reading : readingList) {
            readings.push_back(ConvertReadingToJinja(reading));
        }

        if (asset.Format({{"status", "ok"}, {"period", "Daily Averages"}, {"readings", readings}})) {
            return NRpc::TResponse()
                .SetStatus(NRpc::EHttpCode::Ok)
                .SetAsset(asset);
        } else {
            return NRpc::TResponse().SetStatus(NRpc::EHttpCode::InternalError);
        }
    }

    if (IsAcceptType(request, "application/json")) {
        nlohmann::json jsonResponse = CreateReadingsToJson(readingList, "Daily Averages");
        return NRpc::TResponse()
            .SetStatus(NRpc::EHttpCode::Ok)
            .SetJson(jsonResponse);
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

    if (IsAcceptType(request, "application/json")) {
        nlohmann::json jsonResponse;
        jsonResponse["status"] = "ok";
        jsonResponse["title"] = "Temperature Monitoring System";
        jsonResponse["endpoints"] = {
            {
                {"name", "Raw Temperature Data"},
                {"description", "View the most recent temperature readings captured by the system in real-time"},
                {"url", "/list/raw"}
            },
            {
                {"name", "Hourly Averages"},
                {"description", "See temperature trends aggregated on an hourly basis"},
                {"url", "/list/hour"}
            },
            {
                {"name", "Daily Averages"},
                {"description", "Analyze long-term temperature trends with daily average readings"},
                {"url", "/list/day"}
            }
        };
        
        return NRpc::TResponse()
            .SetStatus(NRpc::EHttpCode::Ok)
            .SetJson(jsonResponse);
    }

    return NRpc::TResponse().SetStatus(NRpc::EHttpCode::BadRequest);
}

NRpc::TResponse TService::HandleAssets(const NRpc::TRequest& request) {
    std::filesystem::path file = request.GetURL().substr(sizeof("/assets"));
    return Assets_->HandleRequest(file);
}

////////////////////////////////////////////////////////////////////////////////

} // namespace NService

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
      Processor_(processor),
      ThreadPool_(NCommon::New<NCommon::TThreadPool>(2)),
      Invoker_(NCommon::New<NCommon::TInvoker>(ThreadPool_))
{
    auto format = NDecode::ParseTemperatureFormat(Config_->SerialConfig->Format);
    Decoder_ = NDecode::CreateDecoder(format);
    Decoder_->SetComPort(Port_);

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
        double value = NAN;

        while (std::isnan(value)) {
            value = Decoder_->ReadTemperature();
        }

        if (auto reading = Processor_(value)) {
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
    
    if (IsAcceptType(request, "application/json")) {
        nlohmann::json jsonResponse = CreateReadingsToJson(readingList, "Raw Data");
        return NRpc::TResponse()
            .SetStatus(NRpc::EHttpCode::Ok)
            .SetJson(jsonResponse)
            .SetHeader("Access-Control-Allow-Origin", "*")
            .SetHeader("Access-Control-Allow-Methods", "GET, OPTIONS")
            .SetHeader("Access-Control-Allow-Headers", "Content-Type, Accept");
    }

    return NRpc::TResponse().SetStatus(NRpc::EHttpCode::BadRequest);
}

NRpc::TResponse TService::HandleHourlyAverages(const NRpc::TRequest& request) {
    auto readingList = Storage_->GetHourlyAverage();
    
    if (IsAcceptType(request, "application/json")) {
        nlohmann::json jsonResponse = CreateReadingsToJson(readingList, "Hourly Averages");
        return NRpc::TResponse()
            .SetStatus(NRpc::EHttpCode::Ok)
            .SetJson(jsonResponse)
            .SetHeader("Access-Control-Allow-Origin", "*")
            .SetHeader("Access-Control-Allow-Methods", "GET, OPTIONS")
            .SetHeader("Access-Control-Allow-Headers", "Content-Type, Accept");
    }
    
    return NRpc::TResponse().SetStatus(NRpc::EHttpCode::BadRequest);
}

NRpc::TResponse TService::HandleDailyAverages(const NRpc::TRequest& request) {
    auto readingList = Storage_->GetDailyAverage();

    if (IsAcceptType(request, "application/json")) {
        nlohmann::json jsonResponse = CreateReadingsToJson(readingList, "Daily Averages");
        return NRpc::TResponse()
            .SetStatus(NRpc::EHttpCode::Ok)
            .SetJson(jsonResponse)
            .SetHeader("Access-Control-Allow-Origin", "*")
            .SetHeader("Access-Control-Allow-Methods", "GET, OPTIONS")
            .SetHeader("Access-Control-Allow-Headers", "Content-Type, Accept");
    }
    
    return NRpc::TResponse().SetStatus(NRpc::EHttpCode::BadRequest);
}

////////////////////////////////////////////////////////////////////////////////

} // namespace NService

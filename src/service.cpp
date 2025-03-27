#include <service.h>
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

////////////////////////////////////////////////////////////////////////////////

namespace {

////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
time_t timegm(struct tm* tm) {
    _tzset();
    return _mkgmtime(tm);
}
#endif

std::string ReadingToString(TReading reading) {
    auto time = std::chrono::system_clock::to_time_t(reading.timestamp);
    std::ostringstream oss;
    oss << std::put_time(gmtime(&time), "%Y-%m-%dT%H:%M:%SZ") << " "
        << std::fixed << std::setprecision(std::numeric_limits<double>::digits10 + 1)
        << reading.temperature;
    return oss.str();
}

TReading StringToReading(const std::string& str) {
    std::istringstream iss(str);
    std::tm tm = {};
    std::string datetime;
    double temp;
    iss >> datetime >> temp;
    
    std::istringstream dtstream(datetime);
    dtstream >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    tm.tm_isdst = 0;
    time_t timestamp = timegm(&tm);
    
    return {
        std::chrono::system_clock::from_time_t(timestamp),
        temp
    };
}

std::deque<TReading> ReadingsFromFile(const std::filesystem::path& file) {
    std::deque<TReading> data;
    try {
        std::fstream fin(file, std::ios::in);
        std::string str;
        while (std::getline(fin, str)) {
            data.emplace_front(StringToReading(str));
        }
    } catch (std::exception& ex) {
        LOG_WARNING("Failed to read file with readings (File: {}, Exception: {})", file, ex);
    }
    LOG_INFO("Got {} from file {}", data.size(), file);
    return data;
}

void ReadingsToFile(const std::filesystem::path& file, const std::deque<TReading>& data) {
    try {
        std::filesystem::create_directory(file.parent_path());
        std::fstream fout(file, std::ios::out);
        for (const auto& reading : data) {
            fout << ReadingToString(reading) << '\n';
        }
    } catch (std::exception& ex) {
        LOG_WARNING("Failed to write readings to file (File: {}, Exception: {})", file, ex);
    }
}

jinja2::ValuesMap ConvertReadingToJinja(const TReading& reading) {
    auto time = std::chrono::system_clock::to_time_t(reading.timestamp);
    std::ostringstream timestampStream;
    timestampStream << std::put_time(std::gmtime(&time), "%Y-%m-%d %H:%M:%S");
    
    return {
        {"timestamp", timestampStream.str()},
        {"temperature", reading.temperature}
    };
}

////////////////////////////////////////////////////////////////////////////////

} // namespace

////////////////////////////////////////////////////////////////////////////////

TFileStorage::TFileStorage(NConfig::TFileStorageConfigPtr config)
    : Config_(config),
      Cache_(NCommon::New<TCache>())
{
    Cache_->rawReadings = ReadingsFromFile(Config_->TemperaturePath);
    Cache_->hourlyAverages = ReadingsFromFile(Config_->TemperatureHourPath);
    Cache_->dailyAverages = ReadingsFromFile(Config_->TemperatureDayPath);
}

void TFileStorage::ProcessTemperature(TReading reading) {
    TCachePtr newCache = NCommon::New<TCache>();
    {
        std::lock_guard<std::mutex> lock(Mutex_);
        newCache->rawReadings = Cache_->rawReadings;
        newCache->hourlyAverages = Cache_->hourlyAverages;
        newCache->dailyAverages = Cache_->dailyAverages;
    }
    
    newCache->rawReadings.push_back(reading);
    
    const auto hour_ago = reading.timestamp - std::chrono::hours(1);
    const auto day_ago = reading.timestamp - std::chrono::days(1);
    const auto month_ago = reading.timestamp - std::chrono::days(30);
    const auto year_ago = reading.timestamp - std::chrono::days(360);

    while (!newCache->rawReadings.empty() && newCache->rawReadings.front().timestamp < day_ago) {
        newCache->rawReadings.pop_front();
    }
    
    while (!newCache->hourlyAverages.empty() && newCache->hourlyAverages.front().timestamp < month_ago) {
        newCache->hourlyAverages.pop_front();
    }

    while (!newCache->dailyAverages.empty() && newCache->dailyAverages.front().timestamp < year_ago) {
        newCache->dailyAverages.pop_front();
    }

    ReadingsToFile(Config_->TemperaturePath, newCache->rawReadings);

// Create hourly average temperature
    bool hourlyUpdate = newCache->hourlyAverages.empty() && newCache->rawReadings.front().timestamp < hour_ago
        || !newCache->hourlyAverages.empty() && newCache->hourlyAverages.back().timestamp < hour_ago;

    if (!hourlyUpdate) {
        std::lock_guard<std::mutex> lock(Mutex_);
        Cache_ = std::move(newCache);
        return;
    }

    double sum = 0;
    size_t count = 0;
    for (auto iter = newCache->rawReadings.rbegin(); iter != newCache->rawReadings.rend() && iter->timestamp >= hour_ago; iter++) {
        count++;
        sum += iter->temperature;
    }
    newCache->hourlyAverages.emplace_back(reading.timestamp, sum / count);

    ReadingsToFile(Config_->TemperatureHourPath, newCache->hourlyAverages);
    
// Create daily average temperature
    bool dailyUpdate = newCache->dailyAverages.empty() && newCache->hourlyAverages.front().timestamp < day_ago
        || !newCache->dailyAverages.empty() && newCache->dailyAverages.back().timestamp < day_ago;

    if (!dailyUpdate) {
        std::lock_guard<std::mutex> lock(Mutex_);
        Cache_ = std::move(newCache);
        return;
    }

    sum = 0;
    count = 0;
    for (auto iter = newCache->hourlyAverages.rbegin(); iter != newCache->hourlyAverages.rend() && iter->timestamp >= day_ago; iter++) {
        count++;
        sum += iter->temperature;
    }
    newCache->dailyAverages.emplace_back(reading.timestamp, sum / count);

    ReadingsToFile(Config_->TemperatureDayPath, newCache->dailyAverages);

    std::lock_guard<std::mutex> lock(Mutex_);
    Cache_ = std::move(newCache);
}

std::vector<TReading> TFileStorage::GetRawReadings() {
    auto cache = Cache_;
    return std::vector<TReading>(cache->rawReadings.rbegin(), cache->rawReadings.rend());
}

std::vector<TReading> TFileStorage::GetHourlyAverage() {
    auto cache = Cache_;
    return std::vector<TReading>(cache->hourlyAverages.rbegin(), cache->hourlyAverages.rend());
}

std::vector<TReading> TFileStorage::GetDailyAverage() {
    auto cache = Cache_;
    return std::vector<TReading>(cache->dailyAverages.rbegin(), cache->dailyAverages.rend());
}

////////////////////////////////////////////////////////////////////////////////

TDataBaseStorage::TDataBaseStorage(NIpc::TDataBaseConfigPtr config)
    : Config_(config),
      Client_(NCommon::New<NIpc::TDbClient>(config))
{
    Client_->Connect();
    CreateTables();
    LoadLastAverages();
}

void TDataBaseStorage::CreateTables() {
    auto tx = Client_->BeginTransaction();
    
    tx.ExecuteQuery(R"(
        CREATE TABLE IF NOT EXISTS raw_temperatures (
            timestamp_ms BIGINT PRIMARY KEY,
            temperature DOUBLE PRECISION NOT NULL
        )
    )");

    tx.ExecuteQuery(R"(
        CREATE TABLE IF NOT EXISTS hourly_averages (
            timestamp_ms BIGINT PRIMARY KEY,
            avg_temperature DOUBLE PRECISION NOT NULL
        )
    )");

    tx.ExecuteQuery(R"(
        CREATE TABLE IF NOT EXISTS daily_averages (
            timestamp_ms BIGINT PRIMARY KEY,
            avg_temperature DOUBLE PRECISION NOT NULL
        )
    )");

    tx.Commit();
}

void TDataBaseStorage::ProcessTemperature(TReading reading) {
    auto tx = Client_->BeginTransaction();
    
    const int64_t tsMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        reading.timestamp.time_since_epoch()
    ).count();

    InsertRawReading(tx, tsMs, reading.temperature);
    
    tx.DeleteRow("raw_temperatures", 
        NCommon::Format("timestamp_ms < {}", tsMs - 86400 * 1000));

    ProcessHourlyAverage(tx, tsMs);
    
    ProcessDailyAverage(tx, tsMs);

    tx.Commit();
}

void TDataBaseStorage::InsertRawReading(NIpc::TTransaction& tx, int64_t tsMs, double temp) {
    tx.InsertRow("raw_temperatures", {
        {"timestamp_ms", std::to_string(tsMs)},
        {"temperature", std::to_string(temp)}
    });
}

void TDataBaseStorage::ProcessHourlyAverage(NIpc::TTransaction& tx, int64_t currentTs) {
    const int64_t oneHour = 3600ll * 1000ll;
    const int64_t monthAgo = currentTs - 30ll * 86400ll * 1000ll;

    if (!LastHourly_ || (currentTs - LastHourly_->TimestampMs) >= oneHour) {
        auto result = tx.ExecuteQuery(
            "INSERT INTO hourly_averages (timestamp_ms, avg_temperature) "
            "SELECT "
            "   MAX(timestamp_ms) AS timestamp_ms, "
            "   AVG(temperature) AS avg_temp "
            "FROM ("
            "   SELECT "
            "       timestamp_ms, "
            "       temperature "
            "   FROM raw_temperatures "
            "   WHERE timestamp_ms BETWEEN $1 AND $2"
            ") as raw",
            currentTs - oneHour,
            currentTs
        );

        if (result.affected_rows() > 0) {
            auto lastResult = tx.SelectRows("hourly_averages", 
                "", {"timestamp_ms DESC"}, 1);
            LastHourly_ = {
                lastResult[0]["timestamp_ms"].as<int64_t>(),
                lastResult[0]["avg_temperature"].as<double>()
            };
        }
    }

    tx.DeleteRow("hourly_averages", 
        NCommon::Format("timestamp_ms < {}", monthAgo));
}

void TDataBaseStorage::ProcessDailyAverage(NIpc::TTransaction& tx, int64_t currentTs) {
    const int64_t oneDay = 86400ll * 1000ll;
    const int64_t yearAgo = currentTs - 365ll * 86400ll * 1000ll;

    if (!LastDaily_ || (currentTs - LastDaily_->TimestampMs) >= oneDay) {
        auto result = tx.ExecuteQuery(
            "INSERT INTO daily_averages (timestamp_ms, avg_temperature) "
            "SELECT "
            "   MAX(timestamp_ms) AS timestamp_ms, "
            "   AVG(avg_temperature) AS avg_temp "
            "FROM ("
            "   SELECT "
            "       timestamp_ms, "
            "       avg_temperature "
            "   FROM hourly_averages "
            "   WHERE timestamp_ms BETWEEN $1 AND $2"
            ") as hourly",
            currentTs - oneDay,
            currentTs
        );

        if (result.affected_rows() > 0) {
            auto lastResult = tx.SelectRows("daily_averages", 
                "", {"timestamp_ms DESC"}, 1);
            LastDaily_ = {
                lastResult[0]["timestamp_ms"].as<int64_t>(),
                lastResult[0]["avg_temperature"].as<double>()
            };
        }
    }

    tx.DeleteRow("daily_averages", 
        NCommon::Format("timestamp_ms < {}", yearAgo));
}

// Helper methods for loading last averages
void TDataBaseStorage::LoadLastAverages() {
    auto getLast = [&](const std::string& table) -> std::optional<AverageRecord> {
        auto result = Client_->SelectRows(table, "", {"timestamp_ms DESC"}, 1);
        if (!result.empty()) {
            return AverageRecord{
                result[0]["timestamp_ms"].as<int64_t>(),
                result[0]["avg_temperature"].as<double>()
            };
        }
        return std::nullopt;
    };

    LastHourly_ = getLast("hourly_averages");
    LastDaily_ = getLast("daily_averages");
}

std::vector<TReading> TDataBaseStorage::ConvertTemperature(const pqxx::result& result) {
    std::vector<TReading> readings;
    for (const auto& row : result) {
        readings.push_back({
            std::chrono::system_clock::time_point(
                std::chrono::milliseconds(row["timestamp_ms"].as<int64_t>())
            ),
                row["temperature"].as<double>()
        });
    }
    return readings;
}

std::vector<TReading> TDataBaseStorage::ConvertAverages(const pqxx::result& result) {
    std::vector<TReading> readings;
    for (const auto& row : result) {
        readings.push_back({
            std::chrono::system_clock::time_point(
                std::chrono::milliseconds(row["timestamp_ms"].as<int64_t>())
            ),
            row["avg_temperature"].as<double>()
        });
    }
    return readings;
}

std::vector<TReading> TDataBaseStorage::GetRawReadings() {
    auto tx = Client_->BeginTransaction();
    auto result = tx.SelectRows("raw_temperatures", "", {"timestamp_ms DESC"});
    return ConvertTemperature(result);
}

std::vector<TReading> TDataBaseStorage::GetHourlyAverage() {
    auto tx = Client_->BeginTransaction();
    auto result = tx.SelectRows("hourly_averages", "", {"timestamp_ms DESC"});
    return ConvertAverages(result);
}

std::vector<TReading> TDataBaseStorage::GetDailyAverage() {
    auto tx = Client_->BeginTransaction();
    auto result = tx.SelectRows("daily_averages", "", {"timestamp_ms DESC"});
    return ConvertAverages(result);
}

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
    auto asset = Assets_->LoadAsset("readings_list.html");
    auto readingList = Storage_->GetRawReadings();
    
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

NRpc::TResponse TService::HandleHourlyAverages(const NRpc::TRequest& request) {
    auto asset = Assets_->LoadAsset("readings_list.html");
    auto readingList = Storage_->GetHourlyAverage();
    
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

NRpc::TResponse TService::HandleDailyAverages(const NRpc::TRequest& request) {
    auto asset = Assets_->LoadAsset("readings_list.html");
    auto readingList = Storage_->GetDailyAverage();
    
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

NRpc::TResponse TService::HandleAssets(const NRpc::TRequest& request) {
    std::filesystem::path file = request.GetURL().substr(sizeof("/assets"));
    return Assets_->HandleRequest(file);
}

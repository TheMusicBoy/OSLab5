#include <service/database_storage.h>

namespace NService {

namespace {

////////////////////////////////////////////////////////////////////////////////

inline const std::string LoggingSource = "DataBaseStorage";

////////////////////////////////////////////////////////////////////////////////

} // namespace

////////////////////////////////////////////////////////////////////////////////

TDataBaseStorage::TDataBaseStorage(NIpc::TDataBaseConfigPtr config)
    : Config_(config),
      Client_(NCommon::New<NIpc::TDbClient>(config))
{
    Client_->Connect();
    auto tx = Client_->BeginTransaction();
    CreateTables();
    LoadLastAverages();
    RefreshCache();
}

void TDataBaseStorage::RefreshCache() {
    while (true) {
        try {
            TCachePtr newCache = NCommon::New<TCache>();

            auto rawResult = Client_->SelectRows("raw_temperatures", "", {"timestamp_ms DESC"});
            newCache->rawReadings = ConvertTemperature(rawResult);

            auto hourlyResult = Client_->SelectRows("hourly_averages", "", {"timestamp_ms DESC"});
            newCache->hourlyAverages = ConvertAverages(hourlyResult);

            auto dailyResult = Client_->SelectRows("daily_averages", "", {"timestamp_ms DESC"});
            newCache->dailyAverages = ConvertAverages(dailyResult);

            Cache_.Store(newCache);
            return;
        } catch (std::exception& ex) {
            LOG_WARNING("Failed to update cache: {}", ex);
        }
    }
}

void TDataBaseStorage::CreateTables() {
    Client_->ExecuteQuery(R"(
        CREATE TABLE IF NOT EXISTS raw_temperatures (
            timestamp_ms BIGINT PRIMARY KEY,
            temperature DOUBLE PRECISION NOT NULL
        )
    )");

    Client_->ExecuteQuery(R"(
        CREATE TABLE IF NOT EXISTS hourly_averages (
            timestamp_ms BIGINT PRIMARY KEY,
            avg_temperature DOUBLE PRECISION NOT NULL
        )
    )");

    Client_->ExecuteQuery(R"(
        CREATE TABLE IF NOT EXISTS daily_averages (
            timestamp_ms BIGINT PRIMARY KEY,
            avg_temperature DOUBLE PRECISION NOT NULL
        )
    )");
}

void TDataBaseStorage::ProcessTemperature(const TReading& reading) {
    try {
        auto tx = Client_->BeginTransaction();

        const int64_t tsMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            reading.timestamp.time_since_epoch()
        ).count();

        InsertRawReading(tsMs, reading.temperature);
        
        Client_->DeleteRow("raw_temperatures", 
            NCommon::Format("timestamp_ms < {}", tsMs - 86400 * 1000));

        ProcessHourlyAverage(tsMs);
        
        ProcessDailyAverage(tsMs);

        RefreshCache();

        tx.Commit();
    } catch (std::exception& ex) {
        LOG_ERROR("Failed to process temperature: {}", ex);
    }
}

void TDataBaseStorage::InsertRawReading(int64_t tsMs, double temp) {
    Client_->InsertRow("raw_temperatures", {
        {"timestamp_ms", std::to_string(tsMs)},
        {"temperature", std::to_string(temp)}
    });
}

void TDataBaseStorage::ProcessHourlyAverage(int64_t currentTs) {
    const int64_t oneHour = 3600ll * 1000ll;
    const int64_t monthAgo = currentTs - 30ll * 86400ll * 1000ll;

    if (!LastHourly_ || (currentTs - LastHourly_->TimestampMs) >= oneHour) {
        auto result = Client_->ExecuteQuery(
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
            auto lastResult = Client_->SelectRows("hourly_averages", 
                "", {"timestamp_ms DESC"}, 1);
            LastHourly_ = {
                lastResult[0]["timestamp_ms"].as<int64_t>(),
                lastResult[0]["avg_temperature"].as<double>()
            };
        }
    }

    Client_->DeleteRow("hourly_averages", 
        NCommon::Format("timestamp_ms < {}", monthAgo));
}

void TDataBaseStorage::ProcessDailyAverage(int64_t currentTs) {
    const int64_t oneDay = 86400ll * 1000ll;
    const int64_t yearAgo = currentTs - 365ll * 86400ll * 1000ll;

    if (!LastDaily_ || (currentTs - LastDaily_->TimestampMs) >= oneDay) {
        auto result = Client_->ExecuteQuery(
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
            auto lastResult = Client_->SelectRows("daily_averages", 
                "", {"timestamp_ms DESC"}, 1);
            LastDaily_ = {
                lastResult[0]["timestamp_ms"].as<int64_t>(),
                lastResult[0]["avg_temperature"].as<double>()
            };
        }
    }

    Client_->DeleteRow("daily_averages", 
        NCommon::Format("timestamp_ms < {}", yearAgo));
}

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

std::deque<TReading> TDataBaseStorage::ConvertTemperature(const pqxx::result& result) {
    std::deque<TReading> readings;
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

std::deque<TReading> TDataBaseStorage::ConvertAverages(const pqxx::result& result) {
    std::deque<TReading> readings;
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

const std::deque<TReading>& TDataBaseStorage::GetRawReadings() {
    return Cache_.Acquire()->rawReadings;
}

const std::deque<TReading>& TDataBaseStorage::GetHourlyAverage() {
    return Cache_.Acquire()->hourlyAverages;
}

const std::deque<TReading>& TDataBaseStorage::GetDailyAverage() {
    return Cache_.Acquire()->dailyAverages;
}

////////////////////////////////////////////////////////////////////////////////

} // namespace NService

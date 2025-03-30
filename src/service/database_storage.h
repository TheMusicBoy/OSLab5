#pragma once

#include <service/storage.h>

#include <ipc/db_client.h>

#include <common/atomic_intrusive_ptr.h>

namespace NService {

////////////////////////////////////////////////////////////////////////////////

class TDataBaseStorage
    : public TTemperatureStorage
{
public:
    TDataBaseStorage(NIpc::TDataBaseConfigPtr config);
    
    const std::deque<TReading>& GetRawReadings() override;
    const std::deque<TReading>& GetHourlyAverage() override;
    const std::deque<TReading>& GetDailyAverage() override;
    
    void ProcessTemperature(const TReading& reading) override;

    void CreateTables();
    void LoadLastAverages();

private:
    struct AverageRecord {
        int64_t TimestampMs;
        double AvgTemperature;
    };
    
    void InsertRawReading(int64_t ts_ms, double temp);
    void ProcessHourlyAverage(int64_t current_ts);
    void ProcessDailyAverage(int64_t current_ts);

    std::deque<TReading> ConvertTemperature(const pqxx::result& result);
    std::deque<TReading> ConvertAverages(const pqxx::result& result);
    
    void RefreshCache();

    std::optional<AverageRecord> LastHourly_;
    std::optional<AverageRecord> LastDaily_;
    
    NIpc::TDataBaseConfigPtr Config_;
    NIpc::TDbClientPtr Client_;
    
    NCommon::TAtomicIntrusivePtr<TCache> Cache_;


};

////////////////////////////////////////////////////////////////////////////////

} // namespace NService

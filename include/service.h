#pragma once

#include <config.h>
#include <assets.h>

#include <common/periodic_executor.h>
#include <common/threadpool.h>
#include <ipc/serial_port.h>
#include <ipc/db_client.h>

////////////////////////////////////////////////////////////////////////////////

struct TReading {
    std::chrono::system_clock::time_point timestamp;
    double temperature;
};

////////////////////////////////////////////////////////////////////////////////

class TTemperatureStorage {
public:
    virtual std::vector<TReading> GetRawReadings() = 0;
    virtual std::vector<TReading> GetHourlyAverage() = 0;
    virtual std::vector<TReading> GetDailyAverage() = 0;

    virtual void ProcessTemperature(TReading reading) = 0;
};

////////////////////////////////////////////////////////////////////////////////

struct TCache
    : NRefCounted::TRefCountedBase
{
    std::deque<TReading> rawReadings;
    std::deque<TReading> hourlyAverages;
    std::deque<TReading> dailyAverages;
};

DECLARE_REFCOUNTED(TCache);

class TFileStorage
    : public TTemperatureStorage
{
public:
    TFileStorage(NConfig::TFileStorageConfigPtr config);

    std::vector<TReading> GetRawReadings();
    std::vector<TReading> GetHourlyAverage();
    std::vector<TReading> GetDailyAverage();

    void ProcessTemperature(TReading reading);

public:
    NConfig::TFileStorageConfigPtr Config_;
    TCachePtr Cache_;
    std::mutex Mutex_;
};

////////////////////////////////////////////////////////////////////////////////

class TDataBaseStorage
    : public TTemperatureStorage
{
public:
    TDataBaseStorage(NIpc::TDataBaseConfigPtr config);
    
    std::vector<TReading> GetRawReadings() override;
    std::vector<TReading> GetHourlyAverage() override;
    std::vector<TReading> GetDailyAverage() override;
    
    void ProcessTemperature(TReading reading) override;

    void CreateTables();
    void LoadLastAverages();

private:
    struct AverageRecord {
        int64_t TimestampMs;
        double AvgTemperature;
    };
    
    void InsertRawReading(NIpc::TTransaction& tx, int64_t ts_ms, double temp);
    void ProcessHourlyAverage(NIpc::TTransaction& tx, int64_t current_ts);
    void ProcessDailyAverage(NIpc::TTransaction& tx, int64_t current_ts);

    std::vector<TReading> ConvertTemperature(const pqxx::result& result);
    std::vector<TReading> ConvertAverages(const pqxx::result& result);

    std::optional<AverageRecord> LastHourly_;
    std::optional<AverageRecord> LastDaily_;
    
    NIpc::TDataBaseConfigPtr Config_;
    NIpc::TDbClientPtr Client_;
};

////////////////////////////////////////////////////////////////////////////////

class TService
    : public NRefCounted::TRefCountedBase
{
private:
    NConfig::TConfigPtr Config_;
    NIpc::TComPortPtr Port_;

    NCommon::TThreadPoolPtr ThreadPool_;
    NCommon::TInvokerPtr Invoker_;

    NCommon::TPeriodicExecutorPtr MesurePeriodicExecutor_;
    NAssets::TAssetsManagerPtr Assets_;

    std::function<std::optional<TReading>(double)> Processor_;
    std::unique_ptr<TTemperatureStorage> Storage_;

    void MesureTemperature();

    void ProcessTemperature(TReading reading);

public:
    TService(NConfig::TConfigPtr config, std::function<std::optional<TReading>(double)> processor);
    ~TService();

    NRpc::TResponse HandleRawReadings(const NRpc::TRequest& request);

    NRpc::TResponse HandleHourlyAverages(const NRpc::TRequest& request);

    NRpc::TResponse HandleDailyAverages(const NRpc::TRequest& request);

    NRpc::TResponse HandleAssets(const NRpc::TRequest& request);

    void Start();

};

DECLARE_REFCOUNTED(TService);

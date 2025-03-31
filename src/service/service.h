#pragma once

#include "config.h"
#include "assets.h"
#include "storage.h"

#include <common/periodic_executor.h>
#include <common/threadpool.h>
#include <ipc/serial_port.h>


namespace NService {

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

    NRpc::TResponse HandleMainPage(const NRpc::TRequest& request);

    NRpc::TResponse HandleAssets(const NRpc::TRequest& request);

    void Start();

};

DECLARE_REFCOUNTED(TService);

////////////////////////////////////////////////////////////////////////////////

} // namespace NService

#pragma once

#include <ui/config.h>
#include <ui/assets.h>

#include <common/periodic_executor.h>
#include <common/threadpool.h>

namespace NService {

////////////////////////////////////////////////////////////////////////////////

class TService
    : public NRefCounted::TRefCountedBase
{
private:
    NConfig::TConfigPtr Config_;

    NAssets::TAssetsManagerPtr Assets_;

public:
    TService(NConfig::TConfigPtr config);
    ~TService();

    NRpc::TResponse HandleRawReadings(const NRpc::TRequest& request);

    NRpc::TResponse HandleHourlyAverages(const NRpc::TRequest& request);

    NRpc::TResponse HandleDailyAverages(const NRpc::TRequest& request);

    NRpc::TResponse HandleMainPage(const NRpc::TRequest& request);

    NRpc::TResponse HandleNotFoundPage(const NRpc::TRequest& request);

    NRpc::TResponse HandleAssets(const NRpc::TRequest& request);

};

DECLARE_REFCOUNTED(TService);

////////////////////////////////////////////////////////////////////////////////

} // namespace NService

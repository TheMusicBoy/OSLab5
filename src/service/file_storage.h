#pragma once

#include <service/storage.h>
#include <service/config.h>
#include <common/atomic_intrusive_ptr.h>

namespace NService {

////////////////////////////////////////////////////////////////////////////////

class TFileStorage
    : public TTemperatureStorage
{
public:
    TFileStorage(NConfig::TFileStorageConfigPtr config);

    const std::deque<TReading>& GetRawReadings() override;
    const std::deque<TReading>& GetHourlyAverage() override;
    const std::deque<TReading>& GetDailyAverage() override;

    void ProcessTemperature(const TReading& reading) override;

public:
    NConfig::TFileStorageConfigPtr Config_;
    NCommon::TAtomicIntrusivePtr<TCache> Cache_;
};

////////////////////////////////////////////////////////////////////////////////

} // namespace NService

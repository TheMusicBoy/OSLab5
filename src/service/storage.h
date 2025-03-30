#pragma once

#include <common/refcounted.h>
#include <common/intrusive_ptr.h>

#include <deque>

////////////////////////////////////////////////////////////////////////////////

struct TReading {
    std::chrono::system_clock::time_point timestamp;
    double temperature;
};

struct TCache
    : NRefCounted::TRefCountedBase
{
    std::deque<TReading> rawReadings;
    std::deque<TReading> hourlyAverages;
    std::deque<TReading> dailyAverages;
};

DECLARE_REFCOUNTED(TCache);

////////////////////////////////////////////////////////////////////////////////

class TTemperatureStorage {
public:
    virtual const std::deque<TReading>& GetRawReadings() = 0;
    virtual const std::deque<TReading>& GetHourlyAverage() = 0;
    virtual const std::deque<TReading>& GetDailyAverage() = 0;

    virtual void ProcessTemperature(const TReading& reading) = 0;
};

////////////////////////////////////////////////////////////////////////////////

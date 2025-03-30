#include <service/file_storage.h>

namespace NService {

namespace {

////////////////////////////////////////////////////////////////////////////////

inline const std::string LoggingSource = "FileStorage";

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

////////////////////////////////////////////////////////////////////////////////

} // namespace

////////////////////////////////////////////////////////////////////////////////

TFileStorage::TFileStorage(NConfig::TFileStorageConfigPtr config)
    : Config_(config)
{
    TCachePtr initialCache = NCommon::New<TCache>();
    initialCache->rawReadings = ReadingsFromFile(Config_->TemperaturePath);
    initialCache->hourlyAverages = ReadingsFromFile(Config_->TemperatureHourPath);
    initialCache->dailyAverages = ReadingsFromFile(Config_->TemperatureDayPath);
    Cache_.Store(initialCache);
}

void TFileStorage::ProcessTemperature(const TReading& reading) {
    TCachePtr currentCache = Cache_.Acquire();
    TCachePtr newCache = NCommon::New<TCache>();
    
    // Copy the existing data
    newCache->rawReadings = currentCache->rawReadings;
    newCache->hourlyAverages = currentCache->hourlyAverages;
    newCache->dailyAverages = currentCache->dailyAverages;
    
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
        Cache_.Store(newCache);
        return;
    }

    // Calculate hourly average
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
        Cache_.Store(newCache);
        return;
    }

    // Calculate daily average
    sum = 0;
    count = 0;
    for (auto iter = newCache->hourlyAverages.rbegin(); iter != newCache->hourlyAverages.rend() && iter->timestamp >= day_ago; iter++) {
        count++;
        sum += iter->temperature;
    }
    newCache->dailyAverages.emplace_back(reading.timestamp, sum / count);

    ReadingsToFile(Config_->TemperatureDayPath, newCache->dailyAverages);

    Cache_.Store(newCache);
}

const std::deque<TReading>& TFileStorage::GetRawReadings() {
    return Cache_.Acquire()->rawReadings;
}

const std::deque<TReading>& TFileStorage::GetHourlyAverage() {
    return Cache_.Acquire()->hourlyAverages;
}

const std::deque<TReading>& TFileStorage::GetDailyAverage() {
    return Cache_.Acquire()->dailyAverages;
}

////////////////////////////////////////////////////////////////////////////////

} // namespace NService


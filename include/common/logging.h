#pragma once

#include <common/format.h>
#include <chrono>
#include <string>
#include <memory>
#include <vector>
#include <mutex>
#include <fstream>
#include <iostream>

namespace NLogging {

////////////////////////////////////////////////////////////////////////////////

enum class ELevel {
    Debug,
    Info,
    Warning,
    Error,
    Fatal
};

std::string LevelToString(ELevel level);

////////////////////////////////////////////////////////////////////////////////

struct TLogEntry {
    std::chrono::system_clock::time_point timestamp;
    ELevel level;
    std::string source;
    std::string message;
    
    TLogEntry(
        std::chrono::system_clock::time_point ts = std::chrono::system_clock::now(),
        ELevel lvl = ELevel::Info,
        std::string src = "",
        std::string msg = ""
    ) : timestamp(ts), level(lvl), source(std::move(src)), message(std::move(msg)) {}
};

class THandler {
public:
    virtual ~THandler() = default;
    
    virtual void Handle(const TLogEntry& entry) = 0;
    
    void SetLevel(ELevel level) {
        level_ = level;
    }
    
    bool ShouldLog(ELevel level) const {
        return level >= level_;
    }
    
protected:
    ELevel level_ = ELevel::Info;
};

class TStreamHandler : public THandler {
public:
    explicit TStreamHandler(std::ostream& stream);
    
    void Handle(const TLogEntry& entry) override;
    
private:
    std::ostream& stream_;
};

class TFileHandler : public THandler {
public:
    explicit TFileHandler(const std::string& filename);
    ~TFileHandler() override;
    
    void Handle(const TLogEntry& entry) override;
    
    void SetMaxFileSize(size_t maxSizeBytes);
    
    void SetMaxBackupCount(size_t count);
    
private:
    void RotateLogFile();
    
    std::ofstream file_;
    std::string filename_;
    size_t maxFileSize_ = 10 * 1024 * 1024; // 10 MB default
    size_t maxBackupCount_ = 5;
    size_t currentFileSize_ = 0;
};

////////////////////////////////////////////////////////////////////////////////

class TLogManager {
public:
    static TLogManager& GetInstance();
    
    void AddHandler(std::shared_ptr<THandler> handler);
    
    void RemoveHandler(std::shared_ptr<THandler> handler);
    
    void Log(const TLogEntry& entry);
    
    template<typename... Args>
    void Log(const std::string& source, ELevel level, const std::string& format, Args&&... args) {
        TLogEntry entry;
        entry.timestamp = std::chrono::system_clock::now();
        entry.level = level;
        entry.source = source;
        entry.message = NCommon::Format(format, std::forward<Args>(args)...);
        
        Log(entry);
    }
    
    template<typename... Args>
    void Debug(const std::string& source, const std::string& format, Args&&... args) {
        Log(source, ELevel::Debug, format, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    void Info(const std::string& source, const std::string& format, Args&&... args) {
        Log(source, ELevel::Info, format, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    void Warning(const std::string& source, const std::string& format, Args&&... args) {
        Log(source, ELevel::Warning, format, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    void Error(const std::string& source, const std::string& format, Args&&... args) {
        Log(source, ELevel::Error, format, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    void Fatal(const std::string& source, const std::string& format, Args&&... args) {
        Log(source, ELevel::Fatal, format, std::forward<Args>(args)...);
    }
    
private:
    TLogManager();
    
    std::vector<std::shared_ptr<THandler>> handlers_;
    std::mutex mutex_;
};

std::shared_ptr<THandler> CreateStdoutHandler();
std::shared_ptr<THandler> CreateStderrHandler();
std::shared_ptr<THandler> CreateFileHandler(const std::string& filename);

inline TLogManager& GetLogManager() {
    return TLogManager::GetInstance();
}

////////////////////////////////////////////////////////////////////////////////

} // namespace NLogging

////////////////////////////////////////////////////////////////////////////////

#define LOG_DEBUG(format, ...) NLogging::GetLogManager().Debug(LoggingSource, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...) NLogging::GetLogManager().Info(LoggingSource, format, ##__VA_ARGS__)
#define LOG_WARNING(format, ...) NLogging::GetLogManager().Warning(LoggingSource, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) NLogging::GetLogManager().Error(LoggingSource, format, ##__VA_ARGS__)
#define LOG_FATAL(format, ...) NLogging::GetLogManager().Fatal(LoggingSource, format, ##__VA_ARGS__)

////////////////////////////////////////////////////////////////////////////////

inline const std::string LoggingSource = "Main";

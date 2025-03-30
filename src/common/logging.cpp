#include <common/logging.h>
#include <filesystem>
#include <thread>

namespace NLogging {

////////////////////////////////////////////////////////////////////////////////

std::string LevelToString(ELevel level) {
    switch (level) {
        case ELevel::Debug:   return "DEBUG";
        case ELevel::Info:    return "INFO";
        case ELevel::Warning: return "WARNING";
        case ELevel::Error:   return "ERROR";
        case ELevel::Fatal:   return "FATAL";
        default:              return "UNKNOWN";
    }
}

////////////////////////////////////////////////////////////////////////////////

TStreamHandler::TStreamHandler(std::ostream& stream) : stream_(stream) {}

void TStreamHandler::Handle(const TLogEntry& entry) {
    if (!ShouldLog(entry.level)) {
        return;
    }
    
    auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
    std::tm tm = *std::localtime(&time_t);
    
    char timeBuffer[32];
    std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &tm);
    
    size_t threadHash = std::hash<std::thread::id>{}(std::this_thread::get_id());
    
    stream_ << timeBuffer << " ["
            << LevelToString(entry.level) << "] ("
            << entry.source << ") "
            << entry.message 
            << "\t[thread:" << std::hex << threadHash << "]"
            << std::endl;
}

////////////////////////////////////////////////////////////////////////////////

TFileHandler::TFileHandler(const std::string& filename)
    : filename_(filename)
{
    file_.open(filename, std::ios::out | std::ios::app);
    if (!file_.is_open()) {
        throw std::runtime_error("Failed to open log file: " + filename);
    }
    
    file_.seekp(0, std::ios::end);
    currentFileSize_ = file_.tellp();
    file_.seekp(0, std::ios::end);
}

TFileHandler::~TFileHandler() {
    if (file_.is_open()) {
        file_.close();
    }
}

void TFileHandler::SetMaxFileSize(size_t maxSizeBytes) {
    maxFileSize_ = maxSizeBytes;
}

void TFileHandler::SetMaxBackupCount(size_t count) {
    maxBackupCount_ = count;
}

void TFileHandler::Handle(const TLogEntry& entry) {
    if (!ShouldLog(entry.level)) {
        return;
    }
    
    auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
    std::tm tm = *std::localtime(&time_t);
    
    char timeBuffer[32];
    std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &tm);
    
    size_t threadHash = std::hash<std::thread::id>{}(std::this_thread::get_id());
    
    std::stringstream messageStream;
    messageStream << timeBuffer << " ["
         << LevelToString(entry.level) << "] ("
         << entry.source << ") "
         << entry.message
         << "\t[thread:" << std::hex << threadHash << "]"
         << std::endl;
                 
    std::string formattedMessage = messageStream.str();
    
    if (currentFileSize_ + formattedMessage.size() > maxFileSize_) {
        RotateLogFile();
    }
    
    file_ << formattedMessage;
    file_.flush();
    currentFileSize_ += formattedMessage.size();
}

void TFileHandler::RotateLogFile() {
    file_.close();
    
    std::string oldestBackup = filename_ + "." + std::to_string(maxBackupCount_);
    if (std::filesystem::exists(oldestBackup)) {
        std::filesystem::remove(oldestBackup);
    }
    
    for (int i = maxBackupCount_ - 1; i > 0; --i) {
        std::string oldName = filename_ + "." + std::to_string(i);
        std::string newName = filename_ + "." + std::to_string(i + 1);
        
        if (std::filesystem::exists(oldName)) {
            std::filesystem::rename(oldName, newName);
        }
    }
    
    std::string backupName = filename_ + ".1";
    std::filesystem::rename(filename_, backupName);
    
    file_.open(filename_, std::ios::out);
    if (!file_.is_open()) {
        throw std::runtime_error("Failed to open new log file after rotation: " + filename_);
    }
    
    currentFileSize_ = 0;
}

////////////////////////////////////////////////////////////////////////////////

TLogManager::TLogManager() {
    AddHandler(CreateStderrHandler());
}

TLogManager& TLogManager::GetInstance() {
    static TLogManager instance;
    return instance;
}

void TLogManager::AddHandler(std::shared_ptr<THandler> handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    handlers_.push_back(std::move(handler));
}

void TLogManager::RemoveHandler(std::shared_ptr<THandler> handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    handlers_.erase(
        std::remove(handlers_.begin(), handlers_.end(), handler),
        handlers_.end()
    );
}

void TLogManager::Log(const TLogEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& handler : handlers_) {
        handler->Handle(entry);
    }
}

std::shared_ptr<THandler> CreateStdoutHandler() {
    return std::make_shared<TStreamHandler>(std::cout);
}

std::shared_ptr<THandler> CreateStderrHandler() {
    return std::make_shared<TStreamHandler>(std::cerr);
}

std::shared_ptr<THandler> CreateFileHandler(const std::string& filename) {
    return std::make_shared<TFileHandler>(filename);
}

////////////////////////////////////////////////////////////////////////////////

} // namespace NLogging

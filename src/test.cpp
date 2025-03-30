#include <common/logging.h>
#include <thread>
#include <vector>
#include <iostream>
#include <filesystem>

int main() {
    std::cout << "Starting logging test...\n";

    // Test 1: Basic console logging
    std::cout << "\n=== Test 1: Basic console logging ===\n";
    
    // Add a stdout handler instead of the default stderr
    auto stdoutHandler = NLogging::CreateStdoutHandler();
    stdoutHandler->SetLevel(NLogging::ELevel::Debug); // Show all logs
    NLogging::GetLogManager().AddHandler(stdoutHandler);

    // Log at different levels
    NLogging::GetLogManager().Debug("Test", "This is a debug message");
    NLogging::GetLogManager().Info("Test", "This is an info message");
    NLogging::GetLogManager().Warning("Test", "This is a warning message");
    NLogging::GetLogManager().Error("Test", "This is an error message");
    NLogging::GetLogManager().Fatal("Test", "This is a fatal message");

    // Test 2: File logging
    std::cout << "\n=== Test 2: File logging ===\n";
    
    const std::string logFilePath = "test_log.log";
    // Delete log file if it exists
    if (std::filesystem::exists(logFilePath)) {
        std::filesystem::remove(logFilePath);
    }
    
    auto fileHandler = NLogging::CreateFileHandler(logFilePath);
    fileHandler->SetLevel(NLogging::ELevel::Info); // Only info and above
    NLogging::GetLogManager().AddHandler(fileHandler);
    
    std::cout << "Logging to file: " << logFilePath << "\n";
    
    NLogging::GetLogManager().Debug("FileTest", "This debug message should NOT appear in the file");
    NLogging::GetLogManager().Info("FileTest", "This info message should appear in the file");
    NLogging::GetLogManager().Error("FileTest", "This error with data: {}", 42);
    
    // Test 3: Log rotation
    std::cout << "\n=== Test 3: Log rotation ===\n";
    
    // Cast to get access to file-specific methods
    auto rotatingFileHandler = NLogging::CreateFileHandler("rotating.log");
    auto* fileHandlerPtr = dynamic_cast<NLogging::TFileHandler*>(rotatingFileHandler.get());
    if (fileHandlerPtr) {
        fileHandlerPtr->SetMaxFileSize(1024); // 1KB max size
        fileHandlerPtr->SetMaxBackupCount(3); // Keep 3 backup files
    }
    
    NLogging::GetLogManager().AddHandler(rotatingFileHandler);
    
    std::cout << "Testing log rotation (check rotating.log files)\n";
    
    // Write enough data to trigger multiple rotations
    for (int i = 0; i < 100; i++) {
        NLogging::GetLogManager().Info("Rotation", "Log line {}: This is a test message that will contribute to log rotation", i);
    }
    
    // Test 4: Thread safety
    std::cout << "\n=== Test 4: Thread safety ===\n";
    
    const int threadCount = 4;
    const int messagesPerThread = 25;
    
    std::vector<std::thread> threads;
    for (int t = 0; t < threadCount; t++) {
        threads.emplace_back([t, messagesPerThread]() {
            for (int i = 0; i < messagesPerThread; i++) {
                NLogging::GetLogManager().Info(
                    "Thread" + std::to_string(t),
                    "Message {} from thread {}", i, t
                );
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    std::cout << "\nLogging test completed successfully!\n";
    return 0;
}

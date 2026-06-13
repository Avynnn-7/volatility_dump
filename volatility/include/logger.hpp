

#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <memory>
#include <chrono>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cstdio>

enum class LogLevel {
    DEBUG = 0,     
    INFO = 1,      
    WARNING = 2,   
    ERROR = 3,     
    CRITICAL = 4   
};

class Logger {
public:
    
    static Logger& getInstance();

    void setLogFile(const std::string& filename);

    void setLogLevel(LogLevel level);

    void enableConsoleOutput(bool enable);

    template<typename... Args>
    void log(LogLevel level, const std::string& format, Args... args) {
        if (level < currentLevel_) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::string message;
        if constexpr (sizeof...(args) == 0) {
            
            message = format;
        } else {
            
            int size = std::snprintf(nullptr, 0, format.c_str(), args...) + 1;
            std::unique_ptr<char[]> buf(new char[size]);
            std::snprintf(buf.get(), size, format.c_str(), args...);
            message = std::string(buf.get(), buf.get() + size - 1);
        }

        std::string logEntry = "[" + getCurrentTimestamp() + "] " + 
                              levelToString(level) + ": " + message;

        if (consoleOutput_) {
            std::cout << logEntry << std::endl;
        }

        if (logFile_.is_open() && logFile_.good()) {
            logFile_ << logEntry << std::endl;
            logFile_.flush();
            
            if (logFile_.fail()) {
                std::cerr << "ERROR: Failed to write to log file. Logging to console only." << std::endl;
                logFile_.close();
            }
        }
    }

    void debug(const std::string& message);

    void info(const std::string& message);

    void warning(const std::string& message);

    void error(const std::string& message);

    void critical(const std::string& message);
    
private:
    Logger() = default;
    std::mutex mutex_;                      
    std::ofstream logFile_;                 
    LogLevel currentLevel_ = LogLevel::INFO; 
    bool consoleOutput_ = true;             
    
    std::string getCurrentTimestamp();
    std::string levelToString(LogLevel level);
};

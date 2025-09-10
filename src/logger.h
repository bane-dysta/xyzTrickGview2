#pragma once

#include <string>
#include <fstream>

// 日志级别枚举
enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR_LEVEL = 3
};

// 简化的日志类（无mutex，适用于cross-compilation）
class Logger {
private:
    std::ofstream logFile;
    LogLevel currentLevel;
    bool logToConsole;
    bool logToFile;

public:
    Logger();
    ~Logger();
    
    bool initialize(const std::string& logFilePath, LogLevel level = LogLevel::INFO);
    void setLogToConsole(bool enabled);
    void setLogToFile(bool enabled);
    void setLogLevel(LogLevel level);
    void log(LogLevel level, const std::string& message, const std::string& file = "", int line = 0);
};

// 字符串转日志级别
LogLevel stringToLogLevel(const std::string& levelStr);

// 全局日志实例
extern Logger g_logger;

// 日志宏定义
#define LOG_DEBUG(msg) g_logger.log(LogLevel::DEBUG, msg, __FILE__, __LINE__)
#define LOG_INFO(msg) g_logger.log(LogLevel::INFO, msg)
#define LOG_WARNING(msg) g_logger.log(LogLevel::WARNING, msg, __FILE__, __LINE__)
#define LOG_ERROR(msg) g_logger.log(LogLevel::ERROR_LEVEL, msg, __FILE__, __LINE__)
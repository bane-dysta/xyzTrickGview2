#include "logger.h"
#include <iostream>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <filesystem>
#include <algorithm>
#include <cctype>

// 全局日志实例
Logger g_logger;

Logger::Logger() : currentLevel(LogLevel::INFO), logToConsole(true), logToFile(true) {}

Logger::~Logger() {
    if (logFile.is_open()) {
        logFile.close();
    }
}

bool Logger::initialize(const std::string& logFilePath, LogLevel level) {
    currentLevel = level;
    
    // 创建日志目录
    std::filesystem::path logPath(logFilePath);
    if (logPath.has_parent_path()) {
        try {
            std::filesystem::create_directories(logPath.parent_path());
        } catch (const std::exception& e) {
            std::cerr << "Failed to create log directory: " << e.what() << std::endl;
        }
    }
    
    logFile.open(logFilePath, std::ios::app);
    if (!logFile.is_open()) {
        std::cerr << "Failed to open log file: " << logFilePath << std::endl;
        logToFile = false;
        return false;
    }
    
    // 写入启动分隔符
    auto now = std::time(nullptr);
    auto* tm = std::localtime(&now);
    logFile << "\n========================================\n";
    logFile << "XYZ Monitor started at: " << std::put_time(tm, "%Y-%m-%d %H:%M:%S") << "\n";
    logFile << "========================================\n";
    logFile.flush();
    
    return true;
}

void Logger::setLogToConsole(bool enabled) {
    logToConsole = enabled;
}

void Logger::setLogToFile(bool enabled) {
    logToFile = enabled;
}

void Logger::setLogLevel(LogLevel level) {
    currentLevel = level;
}

void Logger::log(LogLevel level, const std::string& message, const std::string& file, int line) {
    if (level < currentLevel) return;
    
    // 获取当前时间
    auto now = std::time(nullptr);
    auto* tm = std::localtime(&now);
    
    // 构建日志消息
    std::ostringstream oss;
    oss << "[" << std::put_time(tm, "%Y-%m-%d %H:%M:%S") << "] ";
    
    // 添加日志级别
    switch (level) {
        case LogLevel::DEBUG:   oss << "[DEBUG] "; break;
        case LogLevel::INFO:    oss << "[INFO]  "; break;
        case LogLevel::WARNING: oss << "[WARN]  "; break;
        case LogLevel::ERROR_LEVEL:   oss << "[ERROR] "; break;
    }
    
    oss << message;
    
    // 添加文件和行号信息（用于错误和警告）
    if (!file.empty() && line > 0 && (level >= LogLevel::WARNING)) {
        // 只提取文件名，不包含完整路径
        std::string filename = file;
        size_t lastSlash = filename.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            filename = filename.substr(lastSlash + 1);
        }
        oss << " (" << filename << ":" << line << ")";
    }
    
    std::string logMessage = oss.str();
    
    // 输出到控制台
    if (logToConsole) {
        if (level >= LogLevel::ERROR_LEVEL) {
            std::cerr << logMessage << std::endl;
        } else {
            std::cout << logMessage << std::endl;
        }
    }
    
    // 输出到文件
    if (logToFile && logFile.is_open()) {
        logFile << logMessage << std::endl;
        logFile.flush();
    }
}

// 字符串转日志级别
LogLevel stringToLogLevel(const std::string& levelStr) {
    std::string upper = levelStr;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    
    if (upper == "DEBUG") return LogLevel::DEBUG;
    if (upper == "INFO") return LogLevel::INFO;
    if (upper == "WARNING" || upper == "WARN") return LogLevel::WARNING;
    if (upper == "ERROR") return LogLevel::ERROR_LEVEL;
    
    return LogLevel::INFO; // 默认
}
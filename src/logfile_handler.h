#pragma once

#include <string>

// Log文件类型枚举
enum class LogFileType {
    ORCA,
    GAUSSIAN,
    OTHER
};

// Log文件处理器类
class LogFileHandler {
public:
    // 识别log文件类型
    static LogFileType identifyLogType(const std::string& filepath);
    
    // 使用配置的程序打开log文件
    static bool openLogFile(const std::string& filepath, LogFileType type);
    
private:
    // 读取文件的前N行
    static std::string readFirstLines(const std::string& filepath, int lineCount);
    
    // 检查是否包含特定字符串
    static bool containsString(const std::string& content, const std::string& pattern);
};



#pragma once

#include <string>
#include <windows.h>

// 配置结构体
struct Config {
    std::string hotkey = "CTRL+ALT+X";
    std::string hotkeyReverse = "CTRL+ALT+G";  // 新增：反向转换热键
    std::string gviewPath = "";
    std::string tempDir = "";
    std::string logFile = "logs/xyz_monitor.log";
    std::string gaussianClipboardPath = "";  // 新增：Gaussian clipboard文件路径
    int waitSeconds = 5;
    std::string logLevel = "INFO";
    bool logToConsole = true;
    bool logToFile = true;
    // 新增内存配置项
    int maxMemoryMB = 500;  // 默认500MB
    size_t maxClipboardChars = 0;  // 自动计算，0表示使用内存计算
    
    // XYZ列定义 (1-based索引)
    int elementColumn = 1;  // 元素符号所在列
    int xColumn = 2;        // X坐标所在列
    int yColumn = 3;        // Y坐标所在列
    int zColumn = 4;        // Z坐标所在列
    
    // CHG格式支持
    bool tryParseChgFormat = false;  // 是否尝试以CHG格式解析剪切板文本
};

// 全局配置实例
extern Config g_config;

// 配置相关函数
bool loadConfig(const std::string& configFile);
bool saveConfig(const std::string& configFile);
bool reloadConfiguration();
bool parseHotkey(const std::string& hotkeyStr, UINT& modifiers, UINT& vk);
std::string getExecutableDirectory();
#pragma once

#include <string>
#include <vector>
#include <windows.h>

// 插件结构体
struct Plugin {
    std::string name;           // 插件名称
    std::string cmd;            // 命令
    std::string hotkey;         // 热键（可选）
    bool enabled;              // 是否启用
    UINT hotkeyId;              // 热键ID（内部使用）
    
    Plugin() : enabled(true), hotkeyId(0) {}
};

// 配置结构体
struct Config {
    std::string hotkey = "CTRL+ALT+X";
    std::string hotkeyReverse = "CTRL+ALT+G";
    std::string gviewPath = "";
    std::string tempDir = "";
    std::string logFile = "logs/xyz_monitor.log";
    std::string gaussianClipboardPath = "";
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
    
    // Log文件查看器配置
    std::string orcaLogViewer = "notepad.exe";      // ORCA log文件查看器
    std::string gaussianLogViewer = "gview.exe";     // Gaussian log文件查看器
    std::string otherLogViewer = "notepad.exe";    // 其他log文件查看器
    
    // 插件系统
    std::vector<Plugin> plugins;  // 插件列表
};

// 全局配置实例
extern Config g_config;

// 配置相关函数
bool loadConfig(const std::string& configFile);
bool saveConfig(const std::string& configFile);
bool reloadConfiguration();
bool parseHotkey(const std::string& hotkeyStr, UINT& modifiers, UINT& vk);
std::string getExecutableDirectory();

// --------------------
// Config path utilities
// --------------------
// Last loaded config file path (absolute when possible)
extern std::string g_configFilePath;

// Directory that contains config.ini
std::string getConfigDirectory();

// Expand Windows-style %VAR% environment variables (keeps unknown variables unchanged)
std::string expandEnvironmentVariables(const std::string& input);

// Resolve a file/dir path from config:
// 1) expands %VAR%
// 2) if relative, resolves relative to config.ini directory
std::string resolveConfigPathForFile(const std::string& path);

// Resolve an executable/viewer path from config:
// 1) expands %VAR%
// 2) if relative, first tries "<config_dir>\\<path>"; if it exists, returns absolute
// 3) if it has a parent path (e.g. "bin\\tool.exe"), resolves relative to config.ini directory
// 4) otherwise returns as-is (so Windows can resolve via PATH)
std::string resolveConfigPathForExecutable(const std::string& path);

// 插件相关函数
bool loadPlugins();
bool savePlugins();
bool addPlugin(const std::string& name, const std::string& cmd, const std::string& hotkey = "");
bool removePlugin(const std::string& name);
bool executePlugin(const std::string& name);
bool registerPluginHotkeys();
bool unregisterPluginHotkeys();
#include "config.h"
#include "logger.h"
#include "core.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <filesystem>

// 全局配置实例
Config g_config;

// 读取配置文件
bool loadConfig(const std::string& configFile) {
    std::ifstream file(configFile);
    if (!file.is_open()) {
        // 创建默认配置文件
        std::ofstream outFile(configFile);
        if (outFile.is_open()) {
            std::string exeDir = getExecutableDirectory();
            std::string tempDirPath = exeDir.empty() ? "temp" : exeDir + "\\temp";
            std::string logFilePath = exeDir.empty() ? "logs\\xyz_monitor.log" : exeDir + "\\logs\\xyz_monitor.log";
            
            outFile << "hotkey=CTRL+ALT+X\n";
            outFile << "hotkey_reverse=CTRL+ALT+G\n";
            outFile << "gview_path=gview.exe\n";
            outFile << "gaussian_clipboard_path=Clipboard.frg\n";
            outFile << "temp_dir=" << tempDirPath << "\n";
            outFile << "log_file=" << logFilePath << "\n";
            outFile << "log_level=INFO\n";
            outFile << "log_to_console=true\n";
            outFile << "log_to_file=true\n";
            outFile << "wait_seconds=5\n";
            outFile << "# Memory limit in MB for processing (default: 500MB)\n";
            outFile << "max_memory_mb=500\n";
            outFile << "# Optional: set explicit character limit (0 = auto calculate from memory)\n";
            outFile << "max_clipboard_chars=0\n";
            outFile.close();
            std::cout << "Created default config file: " << configFile << std::endl;
        } else {
            std::cerr << "Failed to create default config file: " << configFile << std::endl;
        }
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = trim(line.substr(0, pos));
            std::string value = trim(line.substr(pos + 1));
            
            try {
                if (key == "hotkey") {
                    g_config.hotkey = value;
                } else if (key == "hotkey_reverse") {
                    g_config.hotkeyReverse = value;
                } else if (key == "gview_path") {
                    g_config.gviewPath = value;
                } else if (key == "gaussian_clipboard_path") {
                    g_config.gaussianClipboardPath = value;
                } else if (key == "temp_dir") {
                    g_config.tempDir = value;
                } else if (key == "log_file") {
                    g_config.logFile = value;
                } else if (key == "log_level") {
                    g_config.logLevel = value;
                } else if (key == "log_to_console") {
                    g_config.logToConsole = (value == "true" || value == "1");
                } else if (key == "log_to_file") {
                    g_config.logToFile = (value == "true" || value == "1");
                } else if (key == "wait_seconds") {
                    g_config.waitSeconds = std::stoi(value);
                } else if (key == "max_memory_mb") {
                    g_config.maxMemoryMB = std::stoi(value);
                    if (g_config.maxMemoryMB < 50) {
                        LOG_WARNING("max_memory_mb is too small (" + std::to_string(g_config.maxMemoryMB) + "), setting to 50MB");
                        g_config.maxMemoryMB = 50;
                    }
                } else if (key == "max_clipboard_chars") {
                    size_t charLimit = std::stoull(value);
                    g_config.maxClipboardChars = charLimit;
                }
            } catch (const std::exception& e) {
                LOG_ERROR("Error parsing config value for key '" + key + "': " + std::string(e.what()));
            }
        }
    }
    file.close();
    
    if (g_config.maxClipboardChars == 0) {
        g_config.maxClipboardChars = calculateMaxChars(g_config.maxMemoryMB);
    }
    
    return true;
}

// 保存配置文件
bool saveConfig(const std::string& configFile) {
    try {
        std::ofstream file(configFile);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open config file for writing: " + configFile);
            return false;
        }
        
        file << "hotkey=" << g_config.hotkey << "\n";
        file << "hotkey_reverse=" << g_config.hotkeyReverse << "\n";
        file << "gview_path=" << g_config.gviewPath << "\n";
        file << "gaussian_clipboard_path=" << g_config.gaussianClipboardPath << "\n";
        file << "temp_dir=" << g_config.tempDir << "\n";
        file << "log_file=" << g_config.logFile << "\n";
        file << "log_level=" << g_config.logLevel << "\n";
        file << "log_to_console=" << (g_config.logToConsole ? "true" : "false") << "\n";
        file << "log_to_file=" << (g_config.logToFile ? "true" : "false") << "\n";
        file << "wait_seconds=" << g_config.waitSeconds << "\n";
        file << "# Memory limit in MB for processing (default: 500MB)\n";
        file << "max_memory_mb=" << g_config.maxMemoryMB << "\n";
        file << "# Optional: set explicit character limit (0 = auto calculate from memory)\n";
        file << "max_clipboard_chars=" << g_config.maxClipboardChars << "\n";
        
        file.close();
        LOG_INFO("Configuration saved to: " + configFile);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Exception saving config: " + std::string(e.what()));
        return false;
    }
}

// 解析热键字符串
bool parseHotkey(const std::string& hotkeyStr, UINT& modifiers, UINT& vk) {
    modifiers = 0;
    vk = 0;
    
    try {
        std::vector<std::string> parts = split(hotkeyStr, '+');
        if (parts.empty()) {
            LOG_ERROR("Empty hotkey string");
            return false;
        }
        
        for (size_t i = 0; i < parts.size() - 1; ++i) {
            std::string mod = parts[i];
            std::transform(mod.begin(), mod.end(), mod.begin(), ::toupper);
            
            if (mod == "CTRL") {
                modifiers |= MOD_CONTROL;
            } else if (mod == "ALT") {
                modifiers |= MOD_ALT;
            } else if (mod == "SHIFT") {
                modifiers |= MOD_SHIFT;
            } else if (mod == "WIN") {
                modifiers |= MOD_WIN;
            } else {
                LOG_ERROR("Unknown modifier: " + mod);
                return false;
            }
        }
        
        std::string key = parts.back();
        std::transform(key.begin(), key.end(), key.begin(), ::toupper);
        
        if (key.length() == 1) {
            vk = key[0];
        } else if (key == "F1") vk = VK_F1;
        else if (key == "F2") vk = VK_F2;
        else if (key == "F3") vk = VK_F3;
        else if (key == "F4") vk = VK_F4;
        else if (key == "F5") vk = VK_F5;
        else if (key == "F6") vk = VK_F6;
        else if (key == "F7") vk = VK_F7;
        else if (key == "F8") vk = VK_F8;
        else if (key == "F9") vk = VK_F9;
        else if (key == "F10") vk = VK_F10;
        else if (key == "F11") vk = VK_F11;
        else if (key == "F12") vk = VK_F12;
        else {
            LOG_ERROR("Unknown key: " + key);
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Exception parsing hotkey: " + std::string(e.what()));
        return false;
    }
}

// 重新加载配置
bool reloadConfiguration() {
    LOG_INFO("Reloading configuration...");
    
    try {
        std::string oldHotkey = g_config.hotkey;
        std::string oldHotkeyReverse = g_config.hotkeyReverse;
        std::string oldLogLevel = g_config.logLevel;
        bool oldLogToConsole = g_config.logToConsole;
        bool oldLogToFile = g_config.logToFile;
        
        std::string exeDir = getExecutableDirectory();
        std::string configPath = exeDir.empty() ? "config.ini" : exeDir + "/config.ini";
        if (!loadConfig(configPath)) {
            LOG_WARNING("Failed to reload config file, using existing configuration");
            return false;
        }
        
        // 更新日志设置
        if (oldLogLevel != g_config.logLevel) {
            LogLevel newLevel = stringToLogLevel(g_config.logLevel);
            g_logger.setLogLevel(newLevel);
            LOG_INFO("Log level changed to: " + g_config.logLevel);
        }
        
        if (oldLogToConsole != g_config.logToConsole) {
            g_logger.setLogToConsole(g_config.logToConsole);
            LOG_INFO("Console logging changed to: " + std::string(g_config.logToConsole ? "enabled" : "disabled"));
        }
        
        if (oldLogToFile != g_config.logToFile) {
            g_logger.setLogToFile(g_config.logToFile);
            LOG_INFO("File logging changed to: " + std::string(g_config.logToFile ? "enabled" : "disabled"));
        }
        
        LOG_INFO("Configuration reloaded successfully");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Exception while reloading configuration: " + std::string(e.what()));
        return false;
    }
}

// 获取可执行文件所在目录
std::string getExecutableDirectory() {
    try {
        char buffer[MAX_PATH];
        DWORD length = GetModuleFileNameA(NULL, buffer, MAX_PATH);
        if (length == 0) {
            LOG_ERROR("Failed to get executable path");
            return "";
        }
        
        std::filesystem::path exePath(buffer);
        std::string exeDir = exePath.parent_path().string();
        LOG_DEBUG("Executable directory: " + exeDir);
        return exeDir;
    } catch (const std::exception& e) {
        LOG_ERROR("Exception getting executable directory: " + std::string(e.what()));
        return "";
    }
}
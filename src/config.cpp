#include "config.h"
#include "logger.h"
#include "core.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <vector>

// 声明外部函数
extern void showTrayNotification(const std::string& title, const std::string& message, DWORD iconType = NIIF_INFO);

// 全局配置实例
Config g_config;

// 最后一次加载的配置文件路径（尽可能为绝对路径）
std::string g_configFilePath;

namespace {

// 配置文件所在目录
std::string g_configDir;

// 根据可执行文件目录，为运行时补齐更合理的默认值（与生成的默认 config.ini 一致）
void applyRuntimeDefaults(Config& cfg) {
    std::string exeDir = getExecutableDirectory();
    if (cfg.gviewPath.empty()) cfg.gviewPath = "gview.exe";
    if (cfg.gaussianClipboardPath.empty()) cfg.gaussianClipboardPath = "Clipboard.frg";

    if (cfg.tempDir.empty()) {
        cfg.tempDir = exeDir.empty() ? "temp" : (exeDir + "\\temp");
    }

    // NOTE: do not override non-empty logFile so users can keep relative paths / variables.
    if (cfg.logFile.empty()) {
        cfg.logFile = exeDir.empty() ? "logs\\xyz_monitor.log" : (exeDir + "\\logs\\xyz_monitor.log");
    }
}

bool writeDefaultConfigFile(const std::string& configFile) {
    std::ofstream outFile(configFile);
    if (!outFile.is_open()) {
        return false;
    }

    std::string exeDir = getExecutableDirectory();
    std::string tempDirPath = exeDir.empty() ? "temp" : exeDir + "\\temp";
    std::string logFilePath = exeDir.empty() ? "logs\\xyz_monitor.log" : exeDir + "\\logs\\xyz_monitor.log";

    outFile << "[main]\n";
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
    outFile << "# XYZ Converter Column Definitions (1-based indexing)\n";
    outFile << "element_column=1\n";
    outFile << "xyz_columns=2,3,4\n";
    outFile << "# CHG Format Support (format: Element X Y Z Charge)\n";
    outFile << "try_parse_chg_format=false\n";
    outFile << "# Log file viewers\n";
    outFile << "orca_log_viewer=notepad.exe\n";
    outFile << "gaussian_log_viewer=gview.exe\n";
    outFile << "other_log_viewer=notepad.exe\n";
    outFile.close();
    return true;
}

} // namespace

// --------------------
// Config path utilities
// --------------------

std::string getConfigDirectory() {
    return g_configDir;
}

// Expand Windows-style %VAR% env vars. Keeps unknown vars as-is.
std::string expandEnvironmentVariables(const std::string& input) {
    if (input.empty()) return input;

    std::string out;
    out.reserve(input.size());

    for (size_t i = 0; i < input.size();) {
        char c = input[i];
        if (c != '%') {
            out.push_back(c);
            ++i;
            continue;
        }

        // Handle escaped %% -> %
        if (i + 1 < input.size() && input[i + 1] == '%') {
            out.push_back('%');
            i += 2;
            continue;
        }

        size_t end = input.find('%', i + 1);
        if (end == std::string::npos) {
            // unmatched %, keep it
            out.push_back('%');
            ++i;
            continue;
        }

        std::string var = input.substr(i + 1, end - (i + 1));
        if (var.empty()) {
            // "%%" already handled above; for "%""%" just keep
            out.append(input.substr(i, end - i + 1));
            i = end + 1;
            continue;
        }

        // Query environment variable
        DWORD required = GetEnvironmentVariableA(var.c_str(), NULL, 0);
        if (required == 0) {
            // not found, keep original %VAR%
            out.append(input.substr(i, end - i + 1));
        } else {
            std::string value;
            value.resize(required);
            DWORD written = GetEnvironmentVariableA(var.c_str(), value.data(), required);
            if (written > 0) {
                // GetEnvironmentVariableA writes without the terminating null when using std::string buffer.
                value.resize(written);
                out.append(value);
            } else {
                // unexpected; keep original
                out.append(input.substr(i, end - i + 1));
            }
        }

        i = end + 1;
    }

    return out;
}

static std::string normalizePathString(const std::filesystem::path& p) {
    try {
        return p.lexically_normal().string();
    } catch (...) {
        return p.string();
    }
}

std::string resolveConfigPathForFile(const std::string& path) {
    std::string expanded = expandEnvironmentVariables(path);
    if (expanded.empty()) return expanded;

    try {
        std::filesystem::path p(expanded);
        if (p.is_absolute()) {
            return normalizePathString(p);
        }

        if (!g_configDir.empty()) {
            std::filesystem::path joined = std::filesystem::path(g_configDir) / p;
            return normalizePathString(joined);
        }

        return normalizePathString(p);
    } catch (...) {
        return expanded;
    }
}

std::string resolveConfigPathForExecutable(const std::string& path) {
    std::string expanded = expandEnvironmentVariables(path);
    if (expanded.empty()) return expanded;

    try {
        std::filesystem::path p(expanded);
        if (p.is_absolute()) {
            return normalizePathString(p);
        }

        // If we know config dir, try resolving relative to it first.
        if (!g_configDir.empty()) {
            std::filesystem::path base(g_configDir);
            std::filesystem::path candidate = base / p;

            // 1) If candidate exists (e.g. gview.exe shipped next to config.ini), use it.
            std::error_code ec;
            if (std::filesystem::exists(candidate, ec)) {
                return normalizePathString(candidate);
            }

            // 2) If the string looks like a relative path with directories, resolve to config dir.
            if (p.has_parent_path()) {
                return normalizePathString(candidate);
            }

            // 3) If user explicitly wrote a relative prefix like .\tool.exe, resolve it.
            if (!expanded.empty() && expanded[0] == '.') {
                return normalizePathString(candidate);
            }
        }

        // Otherwise keep as-is so Windows can resolve via PATH.
        return expanded;
    } catch (...) {
        return expanded;
    }
}

// 读取配置文件
bool loadConfig(const std::string& configFile) {
    // Record config.ini location so that relative paths can be resolved against it.
    try {
        std::filesystem::path cfgPath = std::filesystem::absolute(std::filesystem::path(configFile));
        g_configFilePath = cfgPath.string();
        g_configDir = cfgPath.parent_path().string();
    } catch (...) {
        g_configFilePath = configFile;
        try {
            g_configDir = std::filesystem::path(configFile).parent_path().string();
        } catch (...) {
            g_configDir.clear();
        }
    }
    if (g_configDir.empty()) {
        // Fallback: use current directory
        g_configDir = std::filesystem::current_path().string();
    }

    std::ifstream file(g_configFilePath.empty() ? configFile : g_configFilePath);
    if (!file.is_open()) {
        // 第一次启动：创建默认配置文件，并继续加载（避免“第一次不可用”）
        const std::string& cfgToCreate = g_configFilePath.empty() ? configFile : g_configFilePath;
        if (writeDefaultConfigFile(cfgToCreate)) {
            std::cout << "Created default config file: " << cfgToCreate << std::endl;
        } else {
            std::cerr << "Failed to create default config file: " << cfgToCreate << std::endl;
            return false;
        }

        file.open(cfgToCreate);
        if (!file.is_open()) {
            return false;
        }
    }

    // 每次加载都重置配置，避免 Reload 时插件/热键重复叠加
    g_config = Config{};
    g_config.plugins.clear();
    applyRuntimeDefaults(g_config);
    
    std::string line;
    std::string currentSection = "main";
    
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        // 检查是否是分区标记
        if (line[0] == '[' && line.back() == ']') {
            currentSection = line.substr(1, line.length() - 2);
            continue;
        }
        
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = trim(line.substr(0, pos));
            std::string value = trim(line.substr(pos + 1));
            
            try {
                if (currentSection == "main") {
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
                    } else if (key == "element_column") {
                        g_config.elementColumn = std::stoi(value);
                    } else if (key == "xyz_columns") {
                        std::vector<std::string> parts = split(value, ',');
                        if (parts.size() == 3) {
                            g_config.xColumn = std::stoi(trim(parts[0]));
                            g_config.yColumn = std::stoi(trim(parts[1]));
                            g_config.zColumn = std::stoi(trim(parts[2]));
                        }
                    } else if (key == "try_parse_chg_format") {
                        g_config.tryParseChgFormat = (value == "true" || value == "1");
                    } else if (key == "orca_log_viewer") {
                        g_config.orcaLogViewer = value;
                    } else if (key == "gaussian_log_viewer") {
                        g_config.gaussianLogViewer = value;
                    } else if (key == "other_log_viewer") {
                        g_config.otherLogViewer = value;
                    }
                } else {
                    // 处理插件配置
                    if (key == "cmd") {
                        // 查找或创建插件
                        bool found = false;
                        for (auto& plugin : g_config.plugins) {
                            if (plugin.name == currentSection) {
                                plugin.cmd = value;
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            Plugin newPlugin;
                            newPlugin.name = currentSection;
                            newPlugin.cmd = value;
                            g_config.plugins.push_back(newPlugin);
                        }
                    } else if (key == "hotkey") {
                        // 查找或创建插件
                        bool found = false;
                        for (auto& plugin : g_config.plugins) {
                            if (plugin.name == currentSection) {
                                plugin.hotkey = value;
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            Plugin newPlugin;
                            newPlugin.name = currentSection;
                            newPlugin.hotkey = value;
                            g_config.plugins.push_back(newPlugin);
                        }
                    }
                }
            } catch (const std::exception& e) {
                LOG_ERROR("Error parsing config value for key '" + key + "': " + std::string(e.what()));
            }
        }
    }
    file.close();

    // 如果用户没有在配置里填某些值，则保持（或回退到）运行时默认值
    applyRuntimeDefaults(g_config);
    
    if (g_config.maxClipboardChars == 0) {
        g_config.maxClipboardChars = calculateMaxChars(g_config.maxMemoryMB);
    }

    // 如果配置未显式提供某些字段，则补齐默认值
    applyRuntimeDefaults(g_config);
    
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
        
        // 保存主配置
        file << "[main]\n";
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
        file << "# XYZ Converter Column Definitions (1-based indexing)\n";
        file << "element_column=" << g_config.elementColumn << "\n";
        file << "xyz_columns=" << g_config.xColumn << "," << g_config.yColumn << "," << g_config.zColumn << "\n";
        file << "# CHG Format Support (format: Element X Y Z Charge)\n";
        file << "try_parse_chg_format=" << (g_config.tryParseChgFormat ? "true" : "false") << "\n";
        file << "# Log file viewers\n";
        file << "orca_log_viewer=" << g_config.orcaLogViewer << "\n";
        file << "gaussian_log_viewer=" << g_config.gaussianLogViewer << "\n";
        file << "other_log_viewer=" << g_config.otherLogViewer << "\n";
        
        // 保存插件配置
        for (const auto& plugin : g_config.plugins) {
            if (plugin.enabled) {
                file << "\n[" << plugin.name << "]\n";
                file << "cmd=" << plugin.cmd << "\n";
                if (!plugin.hotkey.empty()) {
                    file << "hotkey=" << plugin.hotkey << "\n";
                }
            }
        }
        
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

// 插件相关函数实现
bool loadPlugins() {
    // 插件在loadConfig中已经加载，这里只是确认
    LOG_INFO("Loaded " + std::to_string(g_config.plugins.size()) + " plugins");
    return true;
}

bool savePlugins() {
    // 插件保存通过saveConfig完成
    return true;
}

bool addPlugin(const std::string& name, const std::string& cmd, const std::string& hotkey) {
    // 检查是否已存在同名插件
    for (auto& plugin : g_config.plugins) {
        if (plugin.name == name) {
            LOG_WARNING("Plugin '" + name + "' already exists, updating...");
            plugin.cmd = cmd;
            plugin.hotkey = hotkey;
            plugin.enabled = true;
            return true;
        }
    }
    
    // 添加新插件
    Plugin newPlugin;
    newPlugin.name = name;
    newPlugin.cmd = cmd;
    newPlugin.hotkey = hotkey;
    newPlugin.enabled = true;
    g_config.plugins.push_back(newPlugin);
    
    LOG_INFO("Added plugin: " + name);
    return true;
}

bool removePlugin(const std::string& name) {
    for (auto it = g_config.plugins.begin(); it != g_config.plugins.end(); ++it) {
        if (it->name == name) {
            g_config.plugins.erase(it);
            LOG_INFO("Removed plugin: " + name);
            return true;
        }
    }
    LOG_WARNING("Plugin not found: " + name);
    return false;
}

bool executePlugin(const std::string& name) {
    for (const auto& plugin : g_config.plugins) {
        if (plugin.name == name && plugin.enabled) {
            LOG_INFO("Executing plugin: " + name + " -> " + plugin.cmd);
            
            try {
                STARTUPINFOA si;
                PROCESS_INFORMATION pi;
                ZeroMemory(&si, sizeof(si));
                si.cb = sizeof(si);
                ZeroMemory(&pi, sizeof(pi));
                
                // CreateProcess 可能会修改命令行缓冲区，因此必须传入可写 buffer
                std::vector<char> cmdBuf(plugin.cmd.begin(), plugin.cmd.end());
                cmdBuf.push_back('\0');

                if (CreateProcessA(NULL, cmdBuf.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                    CloseHandle(pi.hProcess);
                    CloseHandle(pi.hThread);
                    LOG_INFO("Plugin executed successfully: " + name);
                    // 显示成功的气泡通知
                    showTrayNotification("Plugin Executed", "Plugin '" + name + "' executed successfully!", NIIF_INFO);
                    return true;
                } else {
                    DWORD error = GetLastError();
                    LOG_ERROR("Failed to execute plugin '" + name + "' (Error: " + std::to_string(error) + ")");
                    // 显示失败的气泡通知
                    showTrayNotification("Plugin Error", "Failed to execute plugin '" + name + "' (Error: " + std::to_string(error) + ")", NIIF_ERROR);
                    return false;
                }
            } catch (const std::exception& e) {
                LOG_ERROR("Exception executing plugin '" + name + "': " + std::string(e.what()));
                // 显示异常的气泡通知
                showTrayNotification("Plugin Exception", "Exception executing plugin '" + name + "': " + std::string(e.what()), NIIF_ERROR);
                return false;
            }
        }
    }
    LOG_WARNING("Plugin not found or disabled: " + name);
    // 显示未找到插件的气泡通知
    showTrayNotification("Plugin Not Found", "Plugin '" + name + "' not found or disabled!", NIIF_WARNING);
    return false;
}

bool registerPluginHotkeys() {
    extern HWND g_hwnd;
    if (!g_hwnd) return false;
    
    UINT hotkeyId = 100; // 从100开始分配插件热键ID
    for (auto& plugin : g_config.plugins) {
        if (plugin.enabled && !plugin.hotkey.empty()) {
            UINT modifiers, vk;
            if (parseHotkey(plugin.hotkey, modifiers, vk)) {
                if (RegisterHotKey(g_hwnd, hotkeyId, modifiers, vk)) {
                    plugin.hotkeyId = hotkeyId;
                    LOG_INFO("Registered plugin hotkey: " + plugin.name + " -> " + plugin.hotkey);
                } else {
                    DWORD error = GetLastError();
                    LOG_ERROR("Failed to register plugin hotkey for '" + plugin.name + "' (Error: " + std::to_string(error) + ")");
                }
            }
            hotkeyId++;
        }
    }
    return true;
}

bool unregisterPluginHotkeys() {
    extern HWND g_hwnd;
    if (!g_hwnd) return false;
    
    for (auto& plugin : g_config.plugins) {
        if (plugin.hotkeyId != 0) {
            UnregisterHotKey(g_hwnd, plugin.hotkeyId);
            plugin.hotkeyId = 0;
            LOG_DEBUG("Unregistered plugin hotkey: " + plugin.name);
        }
    }
    return true;
}
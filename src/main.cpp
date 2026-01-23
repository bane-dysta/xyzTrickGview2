#include <windows.h>
#include <shellapi.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <ctime>
#include <vector>
#include <chrono>
#include <filesystem>

// 引入自定义模块
#include "core.h"
#include "logger.h"
#include "config.h"
#include "converter.h"
#include "menu.h"
#include "version.h"
#include "logfile_handler.h"

// 解决Windows ERROR宏冲突
#ifdef ERROR
#undef ERROR
#endif

// 资源ID定义
#define IDI_MAIN_ICON 101

// 托盘和菜单常量
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_ICON 1001
#define ID_TRAY_RELOAD 2001
#define ID_TRAY_EXIT 2002
#define ID_TRAY_ABOUT 2003

// 热键ID
#define HOTKEY_XYZ_TO_GVIEW 1
#define HOTKEY_GVIEW_TO_XYZ 2

// 线程参数结构体
struct DeleteFileThreadParams {
    std::string filepath;
    int waitSeconds;
};

// 全局变量
bool g_running = true;
NOTIFYICONDATAA g_nid = {};
HWND g_hwnd = NULL;

// 前置声明
bool reregisterHotkeys();
void cleanupTrayIcon();

// 延时删除文件的线程函数
DWORD WINAPI DeleteFileThread(LPVOID lpParam) {
    DeleteFileThreadParams* params = static_cast<DeleteFileThreadParams*>(lpParam);
    
    try {
        Sleep(params->waitSeconds * 1000);
        
        if (DeleteFileA(params->filepath.c_str())) {
            // 成功删除
        } else {
            DWORD error = GetLastError();
            std::cerr << "Failed to delete temporary file: " << params->filepath << " (Error: " << error << ")" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception in delete file thread: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception in delete file thread" << std::endl;
    }
    
    delete params;
    return 0;
}

// 重新注册热键
bool reregisterHotkeys() {
    if (g_hwnd) {
        // 先注销旧热键
        UnregisterHotKey(g_hwnd, HOTKEY_XYZ_TO_GVIEW);
        UnregisterHotKey(g_hwnd, HOTKEY_GVIEW_TO_XYZ);
        
        // 注册主热键（XYZ到GView）
        UINT modifiers, vk;
        if (parseHotkey(g_config.hotkey, modifiers, vk)) {
            if (RegisterHotKey(g_hwnd, HOTKEY_XYZ_TO_GVIEW, modifiers, vk)) {
                LOG_INFO("Primary hotkey registered: " + g_config.hotkey);
            } else {
                DWORD error = GetLastError();
                LOG_ERROR("Failed to register primary hotkey: " + g_config.hotkey + " (Error: " + std::to_string(error) + ")");
                return false;
            }
        }
        
        // 注册反向热键（GView到XYZ）
        if (parseHotkey(g_config.hotkeyReverse, modifiers, vk)) {
            if (RegisterHotKey(g_hwnd, HOTKEY_GVIEW_TO_XYZ, modifiers, vk)) {
                LOG_INFO("Reverse hotkey registered: " + g_config.hotkeyReverse);
            } else {
                DWORD error = GetLastError();
                LOG_ERROR("Failed to register reverse hotkey: " + g_config.hotkeyReverse + " (Error: " + std::to_string(error) + ")");
                // 主热键已注册，不返回false
            }
        }
        
        return true;
    }
    return false;
}

// 重新加载配置
bool reloadConfigurationWithHotkeys() {
    LOG_INFO("Reloading configuration...");
    
    try {
        std::string oldHotkey = g_config.hotkey;
        std::string oldHotkeyReverse = g_config.hotkeyReverse;
        
        if (!reloadConfiguration()) {
            return false;
        }
        
        // 如果热键改变了，重新注册
        if (oldHotkey != g_config.hotkey || oldHotkeyReverse != g_config.hotkeyReverse) {
            if (reregisterHotkeys()) {
                LOG_INFO("Hotkeys re-registered successfully");
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Exception while reloading configuration: " + std::string(e.what()));
        return false;
    }
}

// 显示托盘通知
void showTrayNotification(const std::string& title, const std::string& message, DWORD iconType = NIIF_INFO) {
    if (g_nid.cbSize > 0) {
        NOTIFYICONDATAA nid = g_nid;
        nid.uFlags = NIF_INFO;
        nid.dwInfoFlags = iconType;  // NIIF_INFO, NIIF_WARNING, NIIF_ERROR
        nid.uTimeout = 3000;  // 显示3秒
        
        strncpy_s(nid.szInfoTitle, sizeof(nid.szInfoTitle), title.c_str(), _TRUNCATE);
        strncpy_s(nid.szInfo, sizeof(nid.szInfo), message.c_str(), _TRUNCATE);
        
        Shell_NotifyIconA(NIM_MODIFY, &nid);
        LOG_DEBUG("Tray notification sent: " + title);
    }
}

// 创建托盘图标
bool createTrayIcon(HWND hwnd) {
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = hwnd;
    g_nid.uID = ID_TRAY_ICON;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    
    g_nid.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAIN_ICON));
    if (!g_nid.hIcon) {
        LOG_WARNING("Failed to load custom icon, using default system icon");
        g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    } else {
        LOG_INFO("Loaded custom icon successfully");
    }
    
    strcpy_s(g_nid.szTip, sizeof(g_nid.szTip), "XYZ Monitor - XYZ<->GView Bridge");
    
    bool result = Shell_NotifyIconA(NIM_ADD, &g_nid);
    if (result) {
        LOG_INFO("System tray icon created");
    } else {
        LOG_ERROR("Failed to create system tray icon");
    }
    
    return result;
}

// 显示托盘菜单
void showTrayMenu(HWND hwnd, POINT pt) {
    HMENU hMenu = CreatePopupMenu();
    if (hMenu) {
        AppendMenuA(hMenu, MF_STRING, ID_TRAY_ABOUT, (APP_NAME " v" VERSION_STRING " - by " APP_AUTHOR));
        AppendMenuA(hMenu, MF_SEPARATOR, 0, NULL);
        
        // 添加插件菜单项
        if (!g_config.plugins.empty()) {
            HMENU hPluginMenu = CreatePopupMenu();
            if (hPluginMenu) {
                for (const auto& plugin : g_config.plugins) {
                    if (plugin.enabled) {
                        std::string menuText = plugin.name;
                        if (!plugin.hotkey.empty()) {
                            menuText += " (" + plugin.hotkey + ")";
                        }
                        AppendMenuA(hPluginMenu, MF_STRING, 2000 + (&plugin - &g_config.plugins[0]), menuText.c_str());
                    }
                }
                AppendMenuA(hMenu, MF_POPUP, (UINT_PTR)hPluginMenu, "Plugins");
            }
            AppendMenuA(hMenu, MF_SEPARATOR, 0, NULL);
        }
        
        AppendMenuA(hMenu, MF_STRING, ID_TRAY_RELOAD, "Reload Configuration");
        AppendMenuA(hMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuA(hMenu, MF_STRING, ID_TRAY_EXIT, "Exit");
        
        SetMenuDefaultItem(hMenu, ID_TRAY_ABOUT, FALSE);
        
        SetForegroundWindow(hwnd);
        TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
        
        DestroyMenu(hMenu);
    }
}

// 清理托盘图标
void cleanupTrayIcon() {
    if (g_nid.cbSize > 0) {
        Shell_NotifyIconA(NIM_DELETE, &g_nid);
        LOG_DEBUG("System tray icon removed");
    }
}


// 读取剪贴板内容
std::string getClipboardText() {
    try {
        if (!OpenClipboard(NULL)) {
            DWORD error = GetLastError();
            LOG_ERROR("Failed to open clipboard (Error: " + std::to_string(error) + ")");
            return "";
        }
        
        HANDLE hData = GetClipboardData(CF_TEXT);
        if (hData == NULL) {
            CloseClipboard();
            LOG_DEBUG("No text data in clipboard");
            return "";
        }
        
        char* pszText = static_cast<char*>(GlobalLock(hData));
        if (pszText == NULL) {
            CloseClipboard();
            LOG_ERROR("Failed to lock clipboard data");
            return "";
        }
        
        std::string text(pszText);
        GlobalUnlock(hData);
        CloseClipboard();
        
        LOG_DEBUG("Clipboard text length: " + std::to_string(text.length()));
        return text;
    } catch (const std::exception& e) {
        LOG_ERROR("Exception reading clipboard: " + std::string(e.what()));
        return "";
    }
}

// 写入剪贴板
bool writeToClipboard(const std::string& text) {
    try {
        if (!OpenClipboard(NULL)) {
            LOG_ERROR("Cannot open clipboard for writing");
            return false;
        }
        
        EmptyClipboard();
        
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.length() + 1);
        if (hMem == NULL) {
            LOG_ERROR("Cannot allocate memory for clipboard");
            CloseClipboard();
            return false;
        }
        
        char* pMem = static_cast<char*>(GlobalLock(hMem));
        if (pMem == NULL) {
            LOG_ERROR("Cannot lock memory for clipboard");
            GlobalFree(hMem);
            CloseClipboard();
            return false;
        }
        
        strcpy_s(pMem, text.length() + 1, text.c_str());
        GlobalUnlock(hMem);
        
        if (SetClipboardData(CF_TEXT, hMem) == NULL) {
            LOG_ERROR("Cannot set clipboard data");
            GlobalFree(hMem);
            CloseClipboard();
            return false;
        }
        
        CloseClipboard();
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Exception writing to clipboard: " + std::string(e.what()));
        return false;
    }
}

// 创建临时文件
std::string createTempFile(const std::string& content) {
    try {
        // 使用更稳妥的唯一文件名，避免同一秒内多次触发导致覆盖
        std::filesystem::path dir;
        if (!g_config.tempDir.empty()) {
            dir = std::filesystem::path(g_config.tempDir);
        } else {
            dir = std::filesystem::temp_directory_path();
        }

        std::filesystem::create_directories(dir);

        const auto now = std::chrono::system_clock::now();
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        const DWORD tid = GetCurrentThreadId();
        // Some MinGW toolchains don't expose GetTickCount64() unless _WIN32_WINNT is set.
        // GetTickCount() is fine for uniqueness here (we already include ms + thread id).
        const ULONGLONG tick = static_cast<ULONGLONG>(GetTickCount());

        std::ostringstream filename;
        filename << "molecule_" << ms << "_" << tick << "_" << tid << ".log";

        std::filesystem::path filepath = dir / filename.str();
        
        std::ofstream file(filepath.string());
        if (!file.is_open()) {
            LOG_ERROR("Failed to create temp file: " + filepath.string());
            return "";
        }
        
        file << content;
        file.close();
        
        LOG_INFO("Created temporary file: " + filepath.string());
        return filepath.string();
    } catch (const std::exception& e) {
        LOG_ERROR("Exception creating temp file: " + std::string(e.what()));
        return "";
    }
}

// 使用GView打开文件
bool openWithGView(const std::string& filepath) {
    try {
        if (g_config.gviewPath.empty()) {
            LOG_ERROR("GView path not configured!");
            return false;
        }
        
        std::string command = "\"" + g_config.gviewPath + "\" \"" + filepath + "\"";
        LOG_DEBUG("Executing command: " + command);
        
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));
        
        // CreateProcess 可能会修改命令行缓冲区，因此必须传入可写 buffer
        std::vector<char> cmdBuf(command.begin(), command.end());
        cmdBuf.push_back('\0');

        if (!CreateProcessA(NULL, cmdBuf.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            DWORD error = GetLastError();
            LOG_ERROR("Failed to launch GView (Error: " + std::to_string(error) + ")");
            return false;
        }
        
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        DeleteFileThreadParams* params = new DeleteFileThreadParams;
        params->filepath = filepath;
        params->waitSeconds = g_config.waitSeconds;
        
        HANDLE hThread = CreateThread(NULL, 0, DeleteFileThread, params, 0, NULL);
        if (hThread) {
            CloseHandle(hThread);
        } else {
            DWORD error = GetLastError();
            LOG_ERROR("Failed to create delete thread (Error: " + std::to_string(error) + ")");
            delete params;
        }
        
        LOG_INFO("Launched GView successfully");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Exception launching GView: " + std::string(e.what()));
        return false;
    }
}

// 处理剪贴板内容（XYZ到GView）
void processClipboardXYZToGView() {
    LOG_INFO("Processing clipboard (XYZ to GView)...");
    
    try {
        std::string content = getClipboardText();
        if (content.empty()) {
            LOG_INFO("Clipboard is empty or not text format.");
            return;
        }
        
        if (content.length() > g_config.maxClipboardChars) {
            LOG_WARNING("Clipboard content is too large (" + std::to_string(content.length()) + 
                       " characters). Limit is " + std::to_string(g_config.maxClipboardChars) + 
                       " characters (" + std::to_string(g_config.maxMemoryMB) + "MB memory limit).");
            return;
        }
        
        // 尝试解析格式
        std::vector<Frame> frames;
        bool isChg = false;
        
        // 如果启用了CHG格式支持，优先尝试CHG格式
        if (g_config.tryParseChgFormat && isChgFormat(content)) {
            LOG_INFO("Detected CHG format in clipboard.");
            isChg = true;
            Frame frame = readChgFrame(content);
            if (!frame.atoms.empty()) {
                frames.push_back(frame);
            }
        } else if (isXYZFormat(content)) {
            LOG_INFO("Detected XYZ format in clipboard.");
            frames = readMultiXYZ(content);
        } else {
            LOG_INFO("Invalid format in clipboard (not XYZ or CHG).");
            return;
        }
        
        double estimatedMemoryMB = (content.length() * 8.0) / (1024.0 * 1024.0);
        LOG_INFO("Processing " + std::to_string(content.length()) + " characters (estimated " + 
                std::to_string(static_cast<int>(estimatedMemoryMB)) + "MB memory usage)");
        if (frames.empty()) {
            LOG_ERROR("Failed to parse XYZ data.");
            return;
        }
        
        LOG_INFO("Found " + std::to_string(frames.size()) + " frame(s) with " + std::to_string(frames[0].atoms.size()) + " atoms.");
        
        std::string gaussianContent = convertToGaussianLog(frames);
        if (gaussianContent.empty()) {
            LOG_ERROR("Failed to convert to Gaussian log format.");
            return;
        }
        
        std::string tempFile = createTempFile(gaussianContent);
        
        if (tempFile.empty()) {
            LOG_ERROR("Failed to create temporary file.");
            return;
        }
        
        if (openWithGView(tempFile)) {
            LOG_INFO("Opened with GView successfully.");
        } else {
            LOG_ERROR("Failed to open with GView.");
            if (!DeleteFileA(tempFile.c_str())) {
                LOG_ERROR("Failed to cleanup temp file: " + tempFile);
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in processClipboardXYZToGView: " + std::string(e.what()));
    } catch (...) {
        LOG_ERROR("Unknown exception in processClipboardXYZToGView");
    }
}

// 处理GView clipboard到XYZ
void processGViewClipboardToXYZ() {
    LOG_INFO("Processing GView clipboard to XYZ...");
    
    try {
        if (g_config.gaussianClipboardPath.empty()) {
            LOG_ERROR("Gaussian clipboard path not configured!");
            showTrayNotification("XYZ Monitor", "Error: Gaussian clipboard path not configured!", NIIF_ERROR);
            return;
        }
        
        // 解析Gaussian clipboard文件
        std::vector<Atom> atoms = parseGaussianClipboard(g_config.gaussianClipboardPath);
        
        if (atoms.empty()) {
            LOG_ERROR("No atoms found in Gaussian clipboard file");
            LOG_INFO("Make sure you have copied a molecule in Gaussian and the path is correct.");
            showTrayNotification("XYZ Monitor", "No atoms found. Copy a molecule in GaussianView first.", NIIF_WARNING);
            return;
        }
        
        LOG_INFO("SUCCESS: Parsed " + std::to_string(atoms.size()) + " atoms");
        
        // 创建XYZ字符串
        std::string xyzString = createXYZString(atoms);
        
        if (xyzString.empty()) {
            LOG_ERROR("Failed to create XYZ string");
            showTrayNotification("XYZ Monitor", "Failed to create XYZ format", NIIF_ERROR);
            return;
        }
        
        // 写入剪贴板
        if (writeToClipboard(xyzString)) {
            LOG_INFO("SUCCESS: XYZ data written to clipboard!");
            LOG_DEBUG("XYZ content preview (first 200 chars): " + xyzString.substr(0, 200) + "...");
            
            // 显示成功通知
            std::string notifMsg = "Converted " + std::to_string(atoms.size()) + " atoms to XYZ format";
            showTrayNotification("GView to XYZ Success", notifMsg, NIIF_INFO);
        } else {
            LOG_ERROR("Failed to write to clipboard");
            showTrayNotification("XYZ Monitor", "Failed to write to clipboard", NIIF_ERROR);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in processGViewClipboardToXYZ: " + std::string(e.what()));
        showTrayNotification("XYZ Monitor", "Error: " + std::string(e.what()), NIIF_ERROR);
    } catch (...) {
        LOG_ERROR("Unknown exception in processGViewClipboardToXYZ");
        showTrayNotification("XYZ Monitor", "Unknown error occurred", NIIF_ERROR);
    }
}

// 窗口过程
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    try {
        switch (uMsg) {
            case WM_HOTKEY:
                if (wParam == HOTKEY_XYZ_TO_GVIEW) {
                    processClipboardXYZToGView();
                } else if (wParam == HOTKEY_GVIEW_TO_XYZ) {
                    processGViewClipboardToXYZ();
                } else if (wParam >= 100) {
                    // 处理插件热键 (ID从100开始)
                    for (const auto& plugin : g_config.plugins) {
                        if (plugin.hotkeyId == wParam) {
                            executePlugin(plugin.name);
                            break;
                        }
                    }
                }
                return 0;
                
            case WM_TRAYICON:
                switch (lParam) {
                    case WM_LBUTTONDBLCLK:
                        ShowMenuWindow();
                        break;
                        
                    case WM_RBUTTONUP:
                        POINT pt;
                        GetCursorPos(&pt);
                        showTrayMenu(hwnd, pt);
                        break;
                }
                return 0;
                
            case WM_COMMAND:
                switch (LOWORD(wParam)) {
                    case ID_TRAY_ABOUT:
                        // 打开设置对话框并切换到About选项卡
                        ShowMenuWindow();
                        if (g_menuWindow) {
                            // 切换到About选项卡（索引为1）
                            TabCtrl_SetCurSel(g_menuWindow->GetTabControl(), 1);
                            g_menuWindow->ShowTab(1);
                        }
                        break;
                        
                    case ID_TRAY_RELOAD:
                        if (reloadConfigurationWithHotkeys()) {
                            MessageBoxA(hwnd, "Configuration reloaded successfully!", "XYZ Monitor", MB_OK | MB_ICONINFORMATION);
                        } else {
                            MessageBoxA(hwnd, "Failed to reload configuration. Check the log file for details.", "XYZ Monitor", MB_OK | MB_ICONERROR);
                        }
                        break;
                        
                    case ID_TRAY_EXIT:
                        g_running = false;
                        PostQuitMessage(0);
                        break;
                        
                    default:
                        // 处理插件菜单项 (ID从2000开始)
                        if (wParam >= 2000) {
                            const int pluginIndex = static_cast<int>(wParam) - 2000;
                            if (pluginIndex >= 0 && static_cast<size_t>(pluginIndex) < g_config.plugins.size()) {
                                executePlugin(g_config.plugins[static_cast<size_t>(pluginIndex)].name);
                            }
                        }
                        break;
                }
                return 0;
                
            case WM_DESTROY:
                cleanupTrayIcon();
                DestroyMenuWindow();
                PostQuitMessage(0);
                return 0;
        }
        
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in WindowProc: " + std::string(e.what()));
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

// 处理文件参数转换功能
bool processFileConversion(const std::string& filepath) {
    LOG_INFO("Processing file conversion: " + filepath);
    
    try {
        // 检查文件是否存在
        if (!std::filesystem::exists(filepath)) {
            LOG_ERROR("File does not exist: " + filepath);
            showTrayNotification("XYZ Monitor", "文件不存在: " + filepath, NIIF_ERROR);
            return false;
        }
        
        // 检查文件扩展名
        std::string ext = std::filesystem::path(filepath).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        
        // 处理log文件（包括 .log 和 .out）
        if (ext == ".log" || ext == ".out") {
            LOG_INFO("Processing log/out file: " + filepath);
            LogFileType logType = LogFileHandler::identifyLogType(filepath);
            
            if (LogFileHandler::openLogFile(filepath, logType)) {
                std::string typeName = (logType == LogFileType::ORCA) ? "ORCA" : 
                                      (logType == LogFileType::GAUSSIAN) ? "Gaussian" : "Other";
                LOG_INFO("Successfully opened " + typeName + " log file: " + filepath);
                showTrayNotification("XYZ Monitor", "成功打开" + typeName + " log文件: " + std::filesystem::path(filepath).filename().string(), NIIF_INFO);
                return true;
            } else {
                LOG_ERROR("Failed to open log file: " + filepath);
                showTrayNotification("XYZ Monitor", "无法打开log文件: " + filepath, NIIF_ERROR);
                return false;
            }
        }
        
        if (ext != ".xyz" && ext != ".trj" && ext != ".chg") {
            LOG_ERROR("Unsupported file format: " + ext);
            showTrayNotification("XYZ Monitor", "不支持的文件格式: " + ext, NIIF_ERROR);
            return false;
        }
        
        // 读取文件内容
        std::ifstream file(filepath);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open file: " + filepath);
            showTrayNotification("XYZ Monitor", "无法打开文件: " + filepath, NIIF_ERROR);
            return false;
        }
        
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        file.close();
        
        if (content.empty()) {
            LOG_ERROR("File is empty: " + filepath);
            showTrayNotification("XYZ Monitor", "文件为空: " + filepath, NIIF_ERROR);
            return false;
        }
        
        // 检查内容长度
        if (content.length() > g_config.maxClipboardChars) {
            LOG_WARNING("File content is too large (" + std::to_string(content.length()) + 
                       " characters). Limit is " + std::to_string(g_config.maxClipboardChars) + 
                       " characters (" + std::to_string(g_config.maxMemoryMB) + "MB memory limit).");
            showTrayNotification("XYZ Monitor", "文件内容过大，超出内存限制", NIIF_WARNING);
            return false;
        }
        
        // 解析文件格式
        std::vector<Frame> frames;
        bool isChg = false;
        
        // 根据扩展名或内容检测格式
        if (ext == ".chg" || (g_config.tryParseChgFormat && isChgFormat(content))) {
            LOG_INFO("Processing CHG format file: " + filepath);
            isChg = true;
            Frame frame = readChgFrame(content);
            if (!frame.atoms.empty()) {
                frames.push_back(frame);
            }
        } else if (isXYZFormat(content)) {
            LOG_INFO("Processing XYZ format file: " + filepath);
            frames = readMultiXYZ(content);
        } else {
            LOG_ERROR("Invalid file format (not XYZ or CHG): " + filepath);
            showTrayNotification("XYZ Monitor", "文件格式无效: " + filepath, NIIF_ERROR);
            return false;
        }
        
        double estimatedMemoryMB = (content.length() * 8.0) / (1024.0 * 1024.0);
        LOG_INFO("Processing " + std::to_string(content.length()) + " characters from file (estimated " + 
                std::to_string(static_cast<int>(estimatedMemoryMB)) + "MB memory usage)");
        if (frames.empty()) {
            LOG_ERROR("Failed to parse XYZ data from file: " + filepath);
            showTrayNotification("XYZ Monitor", "解析XYZ数据失败: " + filepath, NIIF_ERROR);
            return false;
        }
        
        LOG_INFO("Found " + std::to_string(frames.size()) + " frame(s) with " + std::to_string(frames[0].atoms.size()) + " atoms.");
        
        // 转换为Gaussian log格式
        std::string gaussianContent = convertToGaussianLog(frames);
        if (gaussianContent.empty()) {
            LOG_ERROR("Failed to convert file to Gaussian log format: " + filepath);
            showTrayNotification("XYZ Monitor", "转换为Gaussian格式失败: " + filepath, NIIF_ERROR);
            return false;
        }
        
        // 创建临时文件
        std::string tempFile = createTempFile(gaussianContent);
        if (tempFile.empty()) {
            LOG_ERROR("Failed to create temporary file for: " + filepath);
            showTrayNotification("XYZ Monitor", "创建临时文件失败: " + filepath, NIIF_ERROR);
            return false;
        }
        
        // 使用GView打开
        if (openWithGView(tempFile)) {
            LOG_INFO("Successfully opened file with GView: " + filepath);
            showTrayNotification("XYZ Monitor", "成功用GView打开文件: " + std::filesystem::path(filepath).filename().string(), NIIF_INFO);
            return true;
        } else {
            LOG_ERROR("Failed to open file with GView: " + filepath);
            showTrayNotification("XYZ Monitor", "无法用GView打开文件: " + filepath, NIIF_ERROR);
            // 清理临时文件
            if (!DeleteFileA(tempFile.c_str())) {
                LOG_ERROR("Failed to cleanup temp file: " + tempFile);
            }
            return false;
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in processFileConversion: " + std::string(e.what()));
        showTrayNotification("XYZ Monitor", "处理文件时出错: " + std::string(e.what()), NIIF_ERROR);
        return false;
    } catch (...) {
        LOG_ERROR("Unknown exception in processFileConversion");
        showTrayNotification("XYZ Monitor", "处理文件时发生未知错误", NIIF_ERROR);
        return false;
    }
}

int main(int argc, char* argv[]) {
    try {
        // 检查是否有文件参数
        if (argc > 1) {
            std::string filepath = argv[1];
            LOG_INFO("File parameter received: " + filepath);
            
            // 加载配置
            std::string exeDir = getExecutableDirectory();
            std::string configPath = exeDir.empty() ? "config.ini" : exeDir + "/config.ini";
            loadConfig(configPath);
            
            LogLevel logLevel = stringToLogLevel(g_config.logLevel);
            if (!g_logger.initialize(g_config.logFile, logLevel)) {
                std::cerr << "Warning: Failed to initialize log file, logging to console only." << std::endl;
            }
            
            g_logger.setLogToConsole(g_config.logToConsole);
            g_logger.setLogToFile(g_config.logToFile);
            
            // 处理文件转换
            bool success = processFileConversion(filepath);
            return success ? 0 : 1;
        }
        
        // 原有的主程序逻辑
        std::string exeDir = getExecutableDirectory();
        std::string configPath = exeDir.empty() ? "config.ini" : exeDir + "/config.ini";
        loadConfig(configPath);
        
        LogLevel logLevel = stringToLogLevel(g_config.logLevel);
        if (!g_logger.initialize(g_config.logFile, logLevel)) {
            std::cerr << "Warning: Failed to initialize log file, logging to console only." << std::endl;
        }
        
        g_logger.setLogToConsole(g_config.logToConsole);
        g_logger.setLogToFile(g_config.logToFile);
        
        LOG_INFO("XYZ Monitor starting...");
        
        LOG_INFO("Configuration:");
        LOG_INFO("  XYZ->GView Hotkey: " + g_config.hotkey);
        LOG_INFO("  GView->XYZ Hotkey: " + g_config.hotkeyReverse);
        LOG_INFO("  GView Path: " + g_config.gviewPath);
        LOG_INFO("  Gaussian Clipboard: " + g_config.gaussianClipboardPath);
        LOG_INFO("  Temp Dir: " + g_config.tempDir);
        LOG_INFO("  Log File: " + g_config.logFile);
        LOG_INFO("  Log Level: " + g_config.logLevel);
        LOG_INFO("  Wait Seconds: " + std::to_string(g_config.waitSeconds));
        LOG_INFO("  Max Memory: " + std::to_string(g_config.maxMemoryMB) + "MB");
        LOG_INFO("  Max Characters: " + std::to_string(g_config.maxClipboardChars));
        
        // 创建隐藏窗口
        WNDCLASSA wc = {};
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = "XYZMonitorClass";
        wc.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAIN_ICON));
        if (!wc.hIcon) {
            wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        }
        
        if (!RegisterClassA(&wc)) {
            LOG_ERROR("Failed to register window class.");
            return 1;
        }
        
        g_hwnd = CreateWindowExA(
            0, "XYZMonitorClass", "XYZ Monitor", 0,
            0, 0, 0, 0, NULL, NULL, GetModuleHandle(NULL), NULL
        );
        
        if (!g_hwnd) {
            LOG_ERROR("Failed to create window.");
            return 1;
        }
        
        if (!createTrayIcon(g_hwnd)) {
            LOG_WARNING("Failed to create tray icon, continuing without it");
        }
        
        // 注册全局热键
        if (!reregisterHotkeys()) {
            LOG_ERROR("Failed to register hotkeys");
            return 1;
        }
        
        // 注册插件热键
        if (!registerPluginHotkeys()) {
            LOG_WARNING("Failed to register some plugin hotkeys");
        }
        
        LOG_INFO("XYZ Monitor is running. Check system tray for options.");
        LOG_INFO("Press " + g_config.hotkey + " to convert clipboard XYZ to GView.");
        LOG_INFO("Press " + g_config.hotkeyReverse + " to convert GView clipboard to XYZ.");
        
        // 消息循环
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0) && g_running) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        // 清理
        UnregisterHotKey(g_hwnd, HOTKEY_XYZ_TO_GVIEW);
        UnregisterHotKey(g_hwnd, HOTKEY_GVIEW_TO_XYZ);
        unregisterPluginHotkeys();
        cleanupTrayIcon();
        DestroyMenuWindow();
        DestroyWindow(g_hwnd);
        
        LOG_INFO("XYZ Monitor stopped.");
        return 0;
    } catch (const std::exception& e) {
        LOG_ERROR("Fatal exception in main: " + std::string(e.what()));
        return 1;
    } catch (...) {
        LOG_ERROR("Fatal unknown exception in main");
        return 1;
    }
}
#include <windows.h>
#include <shellapi.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <ctime>
#include <filesystem>

// 引入自定义模块
#include "core.h"
#include "logger.h"
#include "config.h"
#include "converter.h"

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
        AppendMenuA(hMenu, MF_STRING, ID_TRAY_ABOUT, "XYZ Monitor v1.1 - by Bane Dysta");
        AppendMenuA(hMenu, MF_SEPARATOR, 0, NULL);
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

// 显示关于对话框
void showAboutDialog(HWND hwnd) {
    std::string message = "XYZ Monitor v1.1\n";
    message += "Author: Bane Dysta\n\n";
    message += "Bidirectional XYZ <-> GView converter.\n\n";
    message += "Current Settings:\n";
    message += "XYZ->GView: " + g_config.hotkey + "\n";
    message += "GView->XYZ: " + g_config.hotkeyReverse + "\n";
    message += "GView Path: " + (g_config.gviewPath.empty() ? "Not configured" : g_config.gviewPath) + "\n";
    message += "Gaussian Clipboard: " + (g_config.gaussianClipboardPath.empty() ? "Not configured" : g_config.gaussianClipboardPath) + "\n";
    message += "Log Level: " + g_config.logLevel + "\n\n";
    message += "Feedback:\n";
    message += "GitHub: https://github.com/bane-dysta/xyzTrickGview\n";
    message += "Forum: http://bbs.keinsci.com/forum.php?mod=viewthread&tid=55596&fromuid=63020\n\n";
    message += "Right-click tray icon for options.";
    
    MessageBoxA(hwnd, message.c_str(), "About XYZ Monitor", MB_OK | MB_ICONINFORMATION);
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
        if (!g_config.tempDir.empty()) {
            std::filesystem::create_directories(g_config.tempDir);
        }
        
        std::time_t t = std::time(nullptr);
        std::ostringstream filename;
        filename << "molecule_" << t << ".log";
        
        std::string filepath = g_config.tempDir.empty() ? filename.str() : g_config.tempDir + "/" + filename.str();
        
        std::ofstream file(filepath);
        if (!file.is_open()) {
            LOG_ERROR("Failed to create temp file: " + filepath);
            return "";
        }
        
        file << content;
        file.close();
        
        LOG_INFO("Created temporary file: " + filepath);
        return filepath;
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
        
        if (!CreateProcessA(NULL, const_cast<char*>(command.c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
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
        
        if (!isXYZFormat(content)) {
            LOG_INFO("Invalid XYZ format in clipboard.");
            return;
        }
        
        double estimatedMemoryMB = (content.length() * 8.0) / (1024.0 * 1024.0);
        LOG_INFO("Processing " + std::to_string(content.length()) + " characters (estimated " + 
                std::to_string(static_cast<int>(estimatedMemoryMB)) + "MB memory usage)");
        
        std::vector<Frame> frames = readMultiXYZ(content);
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
            return;
        }
        
        // 解析Gaussian clipboard文件
        std::vector<Atom> atoms = parseGaussianClipboard(g_config.gaussianClipboardPath);
        
        if (atoms.empty()) {
            LOG_ERROR("No atoms found in Gaussian clipboard file");
            LOG_INFO("Make sure you have copied a molecule in Gaussian and the path is correct.");
            return;
        }
        
        LOG_INFO("SUCCESS: Parsed " + std::to_string(atoms.size()) + " atoms");
        
        // 创建XYZ字符串
        std::string xyzString = createXYZString(atoms);
        
        if (xyzString.empty()) {
            LOG_ERROR("Failed to create XYZ string");
            return;
        }
        
        // 写入剪贴板
        if (writeToClipboard(xyzString)) {
            LOG_INFO("SUCCESS: XYZ data written to clipboard!");
            LOG_DEBUG("XYZ content preview (first 200 chars): " + xyzString.substr(0, 200) + "...");
        } else {
            LOG_ERROR("Failed to write to clipboard");
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in processGViewClipboardToXYZ: " + std::string(e.what()));
    } catch (...) {
        LOG_ERROR("Unknown exception in processGViewClipboardToXYZ");
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
                }
                return 0;
                
            case WM_TRAYICON:
                switch (lParam) {
                    case WM_LBUTTONDBLCLK:
                        showAboutDialog(hwnd);
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
                        showAboutDialog(hwnd);
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
                }
                return 0;
                
            case WM_DESTROY:
                cleanupTrayIcon();
                PostQuitMessage(0);
                return 0;
        }
        
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in WindowProc: " + std::string(e.what()));
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

int main() {
    try {
        loadConfig("config.ini");
        
        LogLevel logLevel = stringToLogLevel(g_config.logLevel);
        if (!g_logger.initialize(g_config.logFile, logLevel)) {
            std::cerr << "Warning: Failed to initialize log file, logging to console only." << std::endl;
        }
        
        g_logger.setLogToConsole(g_config.logToConsole);
        g_logger.setLogToFile(g_config.logToFile);
        
        LOG_INFO("XYZ Monitor v1.1 starting...");
        
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
        cleanupTrayIcon();
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
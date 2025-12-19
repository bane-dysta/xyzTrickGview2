// x86_64-w64-mingw32-g++ xyz2file.cpp -o xyz2file.exe -mwindows -lshell32 -static -O3 -s
#include <windows.h>
#include <string>
#include <fstream>
#include <shlobj.h>
#include <shellapi.h>

// 检查字符串是否是XYZ格式
bool isXYZContent(const std::string& content) {
    if (content.empty()) return false;
    
    size_t firstNewline = content.find('\n');
    if (firstNewline == std::string::npos) return false;
    
    std::string firstLine = content.substr(0, firstNewline);
    
    try {
        size_t pos;
        int atomCount = std::stoi(firstLine, &pos);
        if (pos == firstLine.length() || (pos < firstLine.length() && firstLine[pos] == '\r')) {
            return atomCount > 0;
        }
    } catch (...) {
        return false;
    }
    
    return false;
}

// 从剪切板读取文本
std::string readClipboard() {
    if (!OpenClipboard(nullptr)) {
        return "";
    }
    
    HANDLE hData = GetClipboardData(CF_TEXT);
    if (hData == nullptr) {
        CloseClipboard();
        return "";
    }
    
    char* pszText = static_cast<char*>(GlobalLock(hData));
    if (pszText == nullptr) {
        CloseClipboard();
        return "";
    }
    
    std::string text(pszText);
    GlobalUnlock(hData);
    CloseClipboard();
    
    return text;
}

// 将文件放入剪切板
bool writeFileToClipboard(const std::string& filePath) {
    if (!OpenClipboard(nullptr)) {
        return false;
    }
    
    EmptyClipboard();
    
    int wideSize = MultiByteToWideChar(CP_ACP, 0, filePath.c_str(), -1, nullptr, 0);
    wchar_t* wideFilePath = new wchar_t[wideSize];
    MultiByteToWideChar(CP_ACP, 0, filePath.c_str(), -1, wideFilePath, wideSize);
    
    size_t dropFilesSize = sizeof(DROPFILES) + (wideSize * sizeof(wchar_t)) + sizeof(wchar_t);
    HGLOBAL hGlobal = GlobalAlloc(GHND | GMEM_SHARE, dropFilesSize);
    
    if (hGlobal == nullptr) {
        delete[] wideFilePath;
        CloseClipboard();
        return false;
    }
    
    DROPFILES* pDropFiles = static_cast<DROPFILES*>(GlobalLock(hGlobal));
    if (pDropFiles == nullptr) {
        delete[] wideFilePath;
        GlobalFree(hGlobal);
        CloseClipboard();
        return false;
    }
    
    pDropFiles->pFiles = sizeof(DROPFILES);
    pDropFiles->pt.x = 0;
    pDropFiles->pt.y = 0;
    pDropFiles->fNC = FALSE;
    pDropFiles->fWide = TRUE;
    
    wchar_t* pFiles = reinterpret_cast<wchar_t*>(reinterpret_cast<BYTE*>(pDropFiles) + sizeof(DROPFILES));
    wcscpy_s(pFiles, wideSize, wideFilePath);
    pFiles[wideSize - 1] = L'\0';
    pFiles[wideSize] = L'\0';
    
    GlobalUnlock(hGlobal);
    delete[] wideFilePath;
    
    if (SetClipboardData(CF_HDROP, hGlobal) == nullptr) {
        GlobalFree(hGlobal);
        CloseClipboard();
        return false;
    }
    
    CloseClipboard();
    return true;
}

// 创建隐藏窗口用于通知
HWND createHiddenWindow() {
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = "XYZProcessorClass";
    
    RegisterClassEx(&wc);
    
    return CreateWindowEx(
        0,
        "XYZProcessorClass",
        "XYZ Processor",
        0,
        0, 0, 0, 0,
        HWND_MESSAGE,
        nullptr,
        GetModuleHandle(nullptr),
        nullptr
    );
}

// 显示气泡通知（优化版）
void showNotification(HWND hWnd, const std::string& title, const std::string& message, DWORD flags = NIIF_INFO) {
    NOTIFYICONDATA nid = {0};
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = 1;
    nid.uFlags = NIF_INFO | NIF_MESSAGE;
    nid.dwInfoFlags = flags;
    nid.uCallbackMessage = WM_APP + 1;
    
    strncpy_s(nid.szInfoTitle, title.c_str(), _TRUNCATE);
    strncpy_s(nid.szInfo, message.c_str(), _TRUNCATE);
    
    Shell_NotifyIcon(NIM_ADD, &nid);
    Shell_NotifyIcon(NIM_MODIFY, &nid);
    
    // 不需要等待，立即删除图标（通知会继续显示）
    Sleep(100); // 短暂延迟确保通知显示
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

// Windows GUI程序入口点
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 创建隐藏窗口
    HWND hWnd = createHiddenWindow();
    
    // 读取剪切板内容
    std::string clipboardContent = readClipboard();
    
    if (clipboardContent.empty()) {
        showNotification(hWnd, "XYZ处理器", "剪切板为空", NIIF_WARNING);
        DestroyWindow(hWnd);
        return 1;
    }
    
    // 判断是否是XYZ格式
    if (!isXYZContent(clipboardContent)) {
        showNotification(hWnd, "XYZ处理器", "剪切板内容不是XYZ格式", NIIF_WARNING);
        DestroyWindow(hWnd);
        return 1;
    }
    
    // 获取临时目录路径
    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);
    
    // 生成XYZ文件路径
    std::string xyzFilePath = std::string(tempPath) + "molecule.xyz";
    
    // 写入XYZ文件
    std::ofstream outFile(xyzFilePath, std::ios::binary);
    if (!outFile.is_open()) {
        showNotification(hWnd, "XYZ处理器", "无法创建临时文件", NIIF_ERROR);
        DestroyWindow(hWnd);
        return 1;
    }
    
    outFile << clipboardContent;
    outFile.close();
    
    // 将文件放入剪切板
    if (writeFileToClipboard(xyzFilePath)) {
        showNotification(hWnd, "XYZ处理器", "XYZ文件已创建并复制到剪切板", NIIF_INFO);
    } else {
        showNotification(hWnd, "XYZ处理器", "文件创建成功但无法写入剪切板", NIIF_WARNING);
    }
    
    DestroyWindow(hWnd);
    return 0;
}
#include "menu.h"
#include "config.h"
#include "logger.h"
#include "version.h"
#include <shellapi.h>
#include <commdlg.h>
#include <commctrl.h>
#include <sstream>

// 全局菜单窗口实例
MenuWindow* g_menuWindow = nullptr;

// 控件ID定义
#define ID_TAB_CONTROL 1001
#define ID_HOTKEY_EDIT 1002
#define ID_HOTKEY_REVERSE_EDIT 1003
#define ID_GVIEW_PATH_EDIT 1004
#define ID_GAUSSIAN_CLIPBOARD_EDIT 1005
#define ID_GITHUB_LINK 1007
#define ID_FORUM_LINK 1008
#define ID_APPLY_BUTTON 1009
#define ID_CANCEL_BUTTON 1010
#define ID_OK_BUTTON 1011
#define ID_BROWSE_GVIEW 1012
#define ID_BROWSE_GAUSSIAN 1013

// 选项卡索引
#define TAB_GENERAL 0
#define TAB_ABOUT 1

MenuWindow::MenuWindow(HWND parent) : m_hwnd(nullptr), m_hwndParent(parent), m_font(nullptr) {
    LoadCurrentConfig();
}

MenuWindow::~MenuWindow() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
    }
    if (m_font) {
        DeleteObject(m_font);
    }
}

void MenuWindow::LoadCurrentConfig() {
    m_hotkey = g_config.hotkey;
    m_hotkeyReverse = g_config.hotkeyReverse;
    m_gviewPath = g_config.gviewPath;
    m_gaussianClipboardPath = g_config.gaussianClipboardPath;
    m_logLevel = g_config.logLevel;
}

bool MenuWindow::Show() {
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_SHOW);
        SetForegroundWindow(m_hwnd);
        return true;
    }
    
    // 注册窗口类
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "XYZMonitorMenuClass";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    
    if (!RegisterClassA(&wc)) {
        LOG_ERROR("Failed to register menu window class");
        return false;
    }
    
    // 创建窗口
    m_hwnd = CreateWindowExA(
        WS_EX_TOOLWINDOW,
        "XYZMonitorMenuClass",
        "XYZ Monitor - Settings",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        500, 450,
        m_hwndParent,
        NULL,
        GetModuleHandle(NULL),
        this
    );
    
    if (!m_hwnd) {
        LOG_ERROR("Failed to create menu window");
        return false;
    }
    
    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
    
    return true;
}

LRESULT CALLBACK MenuWindow::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    MenuWindow* pThis = nullptr;
    
    if (uMsg == WM_NCCREATE) {
        CREATESTRUCTA* pCreate = (CREATESTRUCTA*)lParam;
        pThis = (MenuWindow*)pCreate->lpCreateParams;
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        pThis->m_hwnd = hwnd;
    } else {
        pThis = (MenuWindow*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    }
    
    if (pThis) {
        return pThis->HandleMessage(uMsg, wParam, lParam);
    }
    
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

LRESULT MenuWindow::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            OnCreate();
            return 0;
            
        case WM_DESTROY:
            OnDestroy();
            return 0;
            
        case WM_COMMAND:
            OnCommand(wParam, lParam);
            return 0;
            
        case WM_NOTIFY:
            OnNotify(wParam, lParam);
            return 0;
            
        case WM_PAINT:
            OnPaint();
            return 0;
            
        case WM_CLOSE:
            ShowWindow(m_hwnd, SW_HIDE);
            return 0;
    }
    
    return DefWindowProcA(m_hwnd, uMsg, wParam, lParam);
}

void MenuWindow::OnCreate() {
    // 创建字体 - 使用微软雅黑字体，更美观
    m_font = CreateFontA(
        -14,                    // 字体高度
        0,                      // 字体宽度（0表示使用默认比例）
        0,                      // 角度
        0,                      // 基线角度
        FW_NORMAL,              // 字体粗细
        FALSE,                  // 非斜体
        FALSE,                  // 非下划线
        FALSE,                  // 非删除线
        DEFAULT_CHARSET,        // 字符集
        OUT_DEFAULT_PRECIS,     // 输出精度
        CLIP_DEFAULT_PRECIS,    // 裁剪精度
        DEFAULT_QUALITY,        // 输出质量
        DEFAULT_PITCH | FF_DONTCARE, // 间距和字体系列
        "Microsoft YaHei UI"    // 字体名称
    );
    
    CreateTabControl();
    CreateGeneralTab();
    CreateAboutTab();
    
    // 默认显示第一个选项卡
    ShowTab(TAB_GENERAL);
}

void MenuWindow::OnDestroy() {
    g_menuWindow = nullptr;
}

void MenuWindow::OnCommand(WPARAM wParam, LPARAM /*lParam*/) {
    int controlId = LOWORD(wParam);
    // int notificationCode = HIWORD(wParam); // 暂时未使用
    
    switch (controlId) {
        case ID_OK_BUTTON:
            if (ValidateInputs()) {
                ApplySettings();
                ShowWindow(m_hwnd, SW_HIDE);
            }
            break;
            
        case ID_APPLY_BUTTON:
            if (ValidateInputs()) {
                ApplySettings();
            }
            break;
            
        case ID_CANCEL_BUTTON:
            LoadCurrentConfig();
            UpdateControls();
            ShowWindow(m_hwnd, SW_HIDE);
            break;
            
        case ID_BROWSE_GVIEW:
            OnBrowseGViewPath();
            break;
            
        case ID_BROWSE_GAUSSIAN:
            OnBrowseGaussianClipboard();
            break;
            
        case ID_GITHUB_LINK:
            OnOpenLink(FEEDBACK_GITHUB);
            break;
            
        case ID_FORUM_LINK:
            OnOpenLink(FEEDBACK_FORUM);
            break;
    }
}

void MenuWindow::OnNotify(WPARAM /*wParam*/, LPARAM lParam) {
    NMHDR* pnmh = (NMHDR*)lParam;
    
    if (pnmh->idFrom == ID_TAB_CONTROL && pnmh->code == TCN_SELCHANGE) {
        int selectedTab = TabCtrl_GetCurSel(m_tabControl);
        ShowTab(selectedTab);
    }
}

void MenuWindow::OnPaint() {
    PAINTSTRUCT ps;
    BeginPaint(m_hwnd, &ps);
    EndPaint(m_hwnd, &ps);
}

void MenuWindow::CreateTabControl() {
    m_tabControl = CreateWindowA(
        WC_TABCONTROLA,
        "",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        10, 10, 460, 370,
        m_hwnd,
        (HMENU)ID_TAB_CONTROL,
        GetModuleHandle(NULL),
        NULL
    );
    
    if (m_tabControl) {
        // 添加选项卡
        TCITEMA tci = {};
        tci.mask = TCIF_TEXT;
        
        tci.pszText = const_cast<char*>("General");
        TabCtrl_InsertItem(m_tabControl, TAB_GENERAL, &tci);
        
        tci.pszText = const_cast<char*>("About");
        TabCtrl_InsertItem(m_tabControl, TAB_ABOUT, &tci);
    }
}

void MenuWindow::CreateGeneralTab() {
    // 热键设置
    m_hotkeyLabel = CreateWindowA("STATIC", "Primary Hotkey (XYZ->GView):", WS_CHILD | WS_VISIBLE,
                  30, 50, 250, 20, m_hwnd, NULL, GetModuleHandle(NULL), NULL);
    if (m_font) SendMessage(m_hotkeyLabel, WM_SETFONT, (WPARAM)m_font, TRUE);
    
    m_hotkeyEdit = CreateWindowA("EDIT", m_hotkey.c_str(),
                                 WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                                 30, 75, 150, 25, m_hwnd, (HMENU)ID_HOTKEY_EDIT,
                                 GetModuleHandle(NULL), NULL);
    if (m_font) SendMessage(m_hotkeyEdit, WM_SETFONT, (WPARAM)m_font, TRUE);
    
    m_hotkeyReverseLabel = CreateWindowA("STATIC", "Reverse Hotkey (GView->XYZ):", WS_CHILD | WS_VISIBLE,
                  30, 110, 250, 20, m_hwnd, NULL, GetModuleHandle(NULL), NULL);
    if (m_font) SendMessage(m_hotkeyReverseLabel, WM_SETFONT, (WPARAM)m_font, TRUE);
    
    m_hotkeyReverseEdit = CreateWindowA("EDIT", m_hotkeyReverse.c_str(),
                                        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                                        30, 135, 150, 25, m_hwnd, (HMENU)ID_HOTKEY_REVERSE_EDIT,
                                        GetModuleHandle(NULL), NULL);
    if (m_font) SendMessage(m_hotkeyReverseEdit, WM_SETFONT, (WPARAM)m_font, TRUE);
    
    // GView路径设置
    m_gviewPathLabel = CreateWindowA("STATIC", "GView Executable Path:", WS_CHILD | WS_VISIBLE,
                  30, 170, 200, 20, m_hwnd, NULL, GetModuleHandle(NULL), NULL);
    if (m_font) SendMessage(m_gviewPathLabel, WM_SETFONT, (WPARAM)m_font, TRUE);
    
    m_gviewPathEdit = CreateWindowA("EDIT", m_gviewPath.c_str(),
                                    WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                                    30, 195, 300, 25, m_hwnd, (HMENU)ID_GVIEW_PATH_EDIT,
                                    GetModuleHandle(NULL), NULL);
    if (m_font) SendMessage(m_gviewPathEdit, WM_SETFONT, (WPARAM)m_font, TRUE);
    
    m_browseGViewButton = CreateWindowA("BUTTON", "Browse...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                        340, 195, 80, 25, m_hwnd, (HMENU)ID_BROWSE_GVIEW,
                                        GetModuleHandle(NULL), NULL);
    if (m_font) SendMessage(m_browseGViewButton, WM_SETFONT, (WPARAM)m_font, TRUE);
    
    // Gaussian Clipboard路径设置
    m_gaussianClipboardLabel = CreateWindowA("STATIC", "Gaussian Clipboard Path:", WS_CHILD | WS_VISIBLE,
                  30, 230, 200, 20, m_hwnd, NULL, GetModuleHandle(NULL), NULL);
    if (m_font) SendMessage(m_gaussianClipboardLabel, WM_SETFONT, (WPARAM)m_font, TRUE);
    
    m_gaussianClipboardEdit = CreateWindowA("EDIT", m_gaussianClipboardPath.c_str(),
                                            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                                            30, 255, 300, 25, m_hwnd, (HMENU)ID_GAUSSIAN_CLIPBOARD_EDIT,
                                            GetModuleHandle(NULL), NULL);
    if (m_font) SendMessage(m_gaussianClipboardEdit, WM_SETFONT, (WPARAM)m_font, TRUE);
    
    m_browseGaussianButton = CreateWindowA("BUTTON", "Browse...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                           340, 255, 80, 25, m_hwnd, (HMENU)ID_BROWSE_GAUSSIAN,
                                           GetModuleHandle(NULL), NULL);
    if (m_font) SendMessage(m_browseGaussianButton, WM_SETFONT, (WPARAM)m_font, TRUE);
}


void MenuWindow::CreateAboutTab() {
    // 程序信息
    std::string titleText = std::string(APP_NAME) + " v" + VERSION_STRING;
    m_titleLabel = CreateWindowA("STATIC", titleText.c_str(), WS_CHILD | WS_VISIBLE | SS_CENTER,
                  30, 50, 400, 30, m_hwnd, NULL, GetModuleHandle(NULL), NULL);
    if (m_font) SendMessage(m_titleLabel, WM_SETFONT, (WPARAM)m_font, TRUE);
    
    m_authorLabel = CreateWindowA("STATIC", APP_AUTHOR, WS_CHILD | WS_VISIBLE | SS_CENTER,
                  30, 80, 400, 20, m_hwnd, NULL, GetModuleHandle(NULL), NULL);
    if (m_font) SendMessage(m_authorLabel, WM_SETFONT, (WPARAM)m_font, TRUE);
    
    m_descriptionLabel = CreateWindowA("STATIC", APP_DESCRIPTION, WS_CHILD | WS_VISIBLE | SS_CENTER,
                  30, 110, 400, 20, m_hwnd, NULL, GetModuleHandle(NULL), NULL);
    if (m_font) SendMessage(m_descriptionLabel, WM_SETFONT, (WPARAM)m_font, TRUE);
    
    // 链接
    m_linksLabel = CreateWindowA("STATIC", "Links:", WS_CHILD | WS_VISIBLE,
                  30, 150, 100, 20, m_hwnd, NULL, GetModuleHandle(NULL), NULL);
    if (m_font) SendMessage(m_linksLabel, WM_SETFONT, (WPARAM)m_font, TRUE);
    
    m_githubLink = CreateWindowA("BUTTON", "GitHub Repository", 
                                 WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                 30, 175, 150, 25, m_hwnd, (HMENU)ID_GITHUB_LINK,
                                 GetModuleHandle(NULL), NULL);
    if (m_font) SendMessage(m_githubLink, WM_SETFONT, (WPARAM)m_font, TRUE);
    
    m_forumLink = CreateWindowA("BUTTON", "Forum Discussion", 
                                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                200, 175, 150, 25, m_hwnd, (HMENU)ID_FORUM_LINK,
                                GetModuleHandle(NULL), NULL);
    if (m_font) SendMessage(m_forumLink, WM_SETFONT, (WPARAM)m_font, TRUE);
}

void MenuWindow::ShowTab(int tabIndex) {
    // 获取所有子窗口并隐藏它们
    HWND child = GetWindow(m_hwnd, GW_CHILD);
    while (child) {
        // 只隐藏非按钮控件（保留OK、Apply、Cancel按钮）
        if (child != m_okButton && child != m_applyButton && child != m_cancelButton && child != m_tabControl) {
            ShowWindow(child, SW_HIDE);
        }
        child = GetWindow(child, GW_HWNDNEXT);
    }
    
    // 根据选项卡显示相应控件
    switch (tabIndex) {
        case TAB_GENERAL:
            ShowWindow(m_hotkeyLabel, SW_SHOW);
            ShowWindow(m_hotkeyEdit, SW_SHOW);
            ShowWindow(m_hotkeyReverseLabel, SW_SHOW);
            ShowWindow(m_hotkeyReverseEdit, SW_SHOW);
            ShowWindow(m_gviewPathLabel, SW_SHOW);
            ShowWindow(m_gviewPathEdit, SW_SHOW);
            ShowWindow(m_gaussianClipboardLabel, SW_SHOW);
            ShowWindow(m_gaussianClipboardEdit, SW_SHOW);
            ShowWindow(m_browseGViewButton, SW_SHOW);
            ShowWindow(m_browseGaussianButton, SW_SHOW);
            break;
            
        case TAB_ABOUT:
            ShowWindow(m_titleLabel, SW_SHOW);
            ShowWindow(m_authorLabel, SW_SHOW);
            ShowWindow(m_descriptionLabel, SW_SHOW);
            ShowWindow(m_linksLabel, SW_SHOW);
            ShowWindow(m_githubLink, SW_SHOW);
            ShowWindow(m_forumLink, SW_SHOW);
            break;
    }
    
    // 创建按钮（如果还没有）
    if (!m_okButton) {
        m_okButton = CreateWindowA("BUTTON", "OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                   110, 380, 80, 30, m_hwnd, (HMENU)ID_OK_BUTTON,
                                   GetModuleHandle(NULL), NULL);
        if (m_font) SendMessage(m_okButton, WM_SETFONT, (WPARAM)m_font, TRUE);

        m_cancelButton = CreateWindowA("BUTTON", "Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                       200, 380, 80, 30, m_hwnd, (HMENU)ID_CANCEL_BUTTON,
                                       GetModuleHandle(NULL), NULL);
        if (m_font) SendMessage(m_cancelButton, WM_SETFONT, (WPARAM)m_font, TRUE);

        m_applyButton = CreateWindowA("BUTTON", "Apply", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                      290, 380, 80, 30, m_hwnd, (HMENU)ID_APPLY_BUTTON,
                                      GetModuleHandle(NULL), NULL);
        if (m_font) SendMessage(m_applyButton, WM_SETFONT, (WPARAM)m_font, TRUE);
    }
}

void MenuWindow::UpdateControls() {
    if (m_hotkeyEdit) {
        SetWindowTextA(m_hotkeyEdit, m_hotkey.c_str());
    }
    if (m_hotkeyReverseEdit) {
        SetWindowTextA(m_hotkeyReverseEdit, m_hotkeyReverse.c_str());
    }
    if (m_gviewPathEdit) {
        SetWindowTextA(m_gviewPathEdit, m_gviewPath.c_str());
    }
    if (m_gaussianClipboardEdit) {
        SetWindowTextA(m_gaussianClipboardEdit, m_gaussianClipboardPath.c_str());
    }
}

bool MenuWindow::ValidateInputs() {
    char buffer[256];
    
    // 验证热键
    GetWindowTextA(m_hotkeyEdit, buffer, sizeof(buffer));
    std::string hotkey = buffer;
    if (hotkey.empty()) {
        MessageBoxA(m_hwnd, "Primary hotkey cannot be empty!", "Validation Error", MB_OK | MB_ICONERROR);
        return false;
    }
    
    GetWindowTextA(m_hotkeyReverseEdit, buffer, sizeof(buffer));
    std::string hotkeyReverse = buffer;
    if (hotkeyReverse.empty()) {
        MessageBoxA(m_hwnd, "Reverse hotkey cannot be empty!", "Validation Error", MB_OK | MB_ICONERROR);
        return false;
    }
    
    // 验证GView路径
    GetWindowTextA(m_gviewPathEdit, buffer, sizeof(buffer));
    std::string gviewPath = buffer;
    if (!gviewPath.empty()) {
        // 检查文件是否存在
        DWORD attributes = GetFileAttributesA(gviewPath.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            MessageBoxA(m_hwnd, "GView executable path does not exist!", "Validation Error", MB_OK | MB_ICONERROR);
            return false;
        }
    }
    
    // 验证Gaussian Clipboard路径
    GetWindowTextA(m_gaussianClipboardEdit, buffer, sizeof(buffer));
    std::string gaussianClipboardPath = buffer;
    if (!gaussianClipboardPath.empty()) {
        DWORD attributes = GetFileAttributesA(gaussianClipboardPath.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            MessageBoxA(m_hwnd, "Gaussian clipboard path does not exist!", "Validation Error", MB_OK | MB_ICONERROR);
            return false;
        }
    }
    
    return true;
}

void MenuWindow::ApplySettings() {
    char buffer[256];
    
    // 获取输入值
    GetWindowTextA(m_hotkeyEdit, buffer, sizeof(buffer));
    m_hotkey = buffer;
    
    GetWindowTextA(m_hotkeyReverseEdit, buffer, sizeof(buffer));
    m_hotkeyReverse = buffer;
    
    GetWindowTextA(m_gviewPathEdit, buffer, sizeof(buffer));
    m_gviewPath = buffer;
    
    GetWindowTextA(m_gaussianClipboardEdit, buffer, sizeof(buffer));
    m_gaussianClipboardPath = buffer;
    
    // 更新全局配置
    g_config.hotkey = m_hotkey;
    g_config.hotkeyReverse = m_hotkeyReverse;
    g_config.gviewPath = m_gviewPath;
    g_config.gaussianClipboardPath = m_gaussianClipboardPath;
    
    // 保存配置到文件
    if (saveConfig("config.ini")) {
        LOG_INFO("Configuration saved successfully");
        
        // 重新注册热键
        extern bool reregisterHotkeys();
        if (reregisterHotkeys()) {
            LOG_INFO("Hotkeys re-registered successfully");
            MessageBoxA(m_hwnd, "Settings applied successfully!", "Success", MB_OK | MB_ICONINFORMATION);
        } else {
            LOG_ERROR("Failed to re-register hotkeys");
            MessageBoxA(m_hwnd, "Settings saved but failed to re-register hotkeys!", "Warning", MB_OK | MB_ICONWARNING);
        }
    } else {
        LOG_ERROR("Failed to save configuration");
        MessageBoxA(m_hwnd, "Failed to save settings!", "Error", MB_OK | MB_ICONERROR);
    }
}

void MenuWindow::OnBrowseGViewPath() {
    OPENFILENAMEA ofn = {};
    char filename[MAX_PATH] = {};
    
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = "Executable Files (*.exe)\0*.exe\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = "Select GView Executable";
    
    if (GetOpenFileNameA(&ofn)) {
        SetWindowTextA(m_gviewPathEdit, filename);
    }
}

void MenuWindow::OnBrowseGaussianClipboard() {
    OPENFILENAMEA ofn = {};
    char filename[MAX_PATH] = {};
    
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = "All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = "Select Gaussian Clipboard File";
    
    if (GetOpenFileNameA(&ofn)) {
        SetWindowTextA(m_gaussianClipboardEdit, filename);
    }
}

void MenuWindow::OnOpenLink(const std::string& url) {
    ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

// 全局函数实现
bool CreateMenuWindow(HWND parent) {
    if (g_menuWindow) {
        return true;
    }
    
    g_menuWindow = new MenuWindow(parent);
    return g_menuWindow != nullptr;
}

void DestroyMenuWindow() {
    if (g_menuWindow) {
        delete g_menuWindow;
        g_menuWindow = nullptr;
    }
}

bool IsMenuWindowVisible() {
    return g_menuWindow && g_menuWindow->GetHandle() && IsWindowVisible(g_menuWindow->GetHandle());
}

void ShowMenuWindow() {
    if (!g_menuWindow) {
        CreateMenuWindow(GetActiveWindow());
    }
    
    if (g_menuWindow) {
        g_menuWindow->Show();
    }
}

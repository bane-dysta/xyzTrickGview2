#pragma once

#include <windows.h>
#include <commctrl.h>
#include <string>

// 菜单窗口类
class MenuWindow {
private:
    HWND m_hwnd;
    HWND m_hwndParent;
    HWND m_tabControl;
    
    // General tab controls
    HWND m_hotkeyEdit;
    HWND m_hotkeyReverseEdit;
    HWND m_gviewPathEdit;
    HWND m_gaussianClipboardEdit;
    HWND m_browseGViewButton;
    HWND m_browseGaussianButton;
    HWND m_openLogButton;
    
    // General tab labels
    HWND m_hotkeyLabel;
    HWND m_hotkeyReverseLabel;
    HWND m_gviewPathLabel;
    HWND m_gaussianClipboardLabel;
    
    // About tab controls
    HWND m_githubLink;
    HWND m_forumLink;
    
    // About tab labels
    HWND m_titleLabel;
    HWND m_authorLabel;
    HWND m_descriptionLabel;
    HWND m_linksLabel;
    
    // Control tab controls
    HWND m_elementColumnEdit;
    HWND m_xColumnEdit;
    HWND m_yColumnEdit;
    HWND m_zColumnEdit;
    HWND m_chgFormatCheckbox;
    
    // Control tab labels
    HWND m_controlDescLabel;
    HWND m_elementColumnLabel;
    HWND m_xyzColumnsLabel;
    
    // Plugins tab controls
    HWND m_pluginListBox;
    HWND m_pluginNameEdit;
    HWND m_pluginCmdEdit;
    HWND m_pluginHotkeyEdit;
    HWND m_addPluginButton;
    HWND m_removePluginButton;
    HWND m_testPluginButton;
    
    // Plugins tab labels
    HWND m_pluginListLabel;
    HWND m_pluginNameLabel;
    HWND m_pluginCmdLabel;
    HWND m_pluginHotkeyLabel;
    
    // Common controls
    HWND m_applyButton;
    HWND m_cancelButton;
    HWND m_okButton;
    
    // Font
    HFONT m_font;
    
    // 当前配置的副本
    std::string m_hotkey;
    std::string m_hotkeyReverse;
    std::string m_gviewPath;
    std::string m_gaussianClipboardPath;
    std::string m_logLevel;
    int m_elementColumn;
    int m_xColumn;
    int m_yColumn;
    int m_zColumn;
    bool m_tryParseChgFormat;
    
    // 窗口过程
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
    
    // 消息处理函数
    void OnCreate();
    void OnDestroy();
    void OnCommand(WPARAM wParam, LPARAM lParam);
    void OnNotify(WPARAM wParam, LPARAM lParam);
    void OnPaint();
    
    // 控件创建函数
    void CreateTabControl();
    void CreateGeneralTab();
    void CreateControlTab();
    void CreateAboutTab();
    void CreatePluginsTab();
    
    // 工具函数
    void UpdateControls();
    void LoadCurrentConfig();
    bool ValidateInputs();
    void ApplySettings();
    
    // 对话框处理
    void OnBrowseGViewPath();
    void OnBrowseGaussianClipboard();
    void OnOpenLogFile();
    void OnOpenLink(const std::string& url);
    
    // 插件管理函数
    void OnAddPlugin();
    void OnRemovePlugin();
    void OnTestPlugin();
    void OnPluginSelectionChanged();
    void UpdatePluginList();
    
public:
    MenuWindow(HWND parent);
    ~MenuWindow();
    
    // 显示菜单窗口
    bool Show();
    
    // 获取窗口句柄
    HWND GetHandle() const { return m_hwnd; }
    
    // 获取选项卡控件句柄
    HWND GetTabControl() const { return m_tabControl; }
    
    // 显示指定选项卡
    void ShowTab(int tabIndex);
};

// 全局菜单窗口实例
extern MenuWindow* g_menuWindow;

// 菜单相关函数
bool CreateMenuWindow(HWND parent);
void DestroyMenuWindow();
bool IsMenuWindowVisible();
void ShowMenuWindow();

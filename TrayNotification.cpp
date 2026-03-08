#include "TrayNotification.h"
#include <shellapi.h>

// Tray callback message ID
const UINT WM_TRAYICON = WM_APP + 1;

// Tray menu command IDs
const UINT_PTR IDM_EXIT = 40001;

// Global tray state
static NOTIFYICONDATAW g_nid = {};
static bool g_notifyIconAdded = false;
static HWND g_notifyHwnd = NULL;

// Set the window handle used for tray notifications
void SetTrayWindowHandle(HWND hwnd)
{
    g_notifyHwnd = hwnd;
}

// Get the window handle
HWND GetTrayWindowHandle()
{
    return g_notifyHwnd;
}

// Check if tray icon is already added
bool IsTrayIconAdded()
{
    return g_notifyIconAdded;
}

// Initialize tray icon at startup
void InitializeTrayIcon()
{
    if (g_notifyIconAdded) return;

    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = g_notifyHwnd;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.uID = 1001;
    g_nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    g_nid.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    wcscpy_s(g_nid.szTip, _countof(g_nid.szTip), L"Headphones disconnected");

    if (Shell_NotifyIconW(NIM_ADD, &g_nid)) {
        g_notifyIconAdded = true;
        OutputDebugStringW(L"InitializeTrayIcon: notification icon added\n");
    } else {
        OutputDebugStringW(L"InitializeTrayIcon: failed to add notification icon\n");
    }
}

// Update tray tooltip
void UpdateTrayTooltip(const std::wstring& tooltip)
{
    if (!g_notifyIconAdded) {
        InitializeTrayIcon();
    }

    wcscpy_s(g_nid.szTip, _countof(g_nid.szTip), tooltip.c_str());
    g_nid.uFlags = NIF_TIP;
    if (!Shell_NotifyIconW(NIM_MODIFY, &g_nid)) {
        OutputDebugStringW(L"UpdateTrayTooltip: failed to update tooltip\n");
    }
}

// Show a non-blocking balloon notification using the system tray
void ShowBalloonNotification(const std::wstring& title, const std::wstring& message)
{
    // Initialize tray icon if needed
    if (!g_notifyIconAdded) {
        InitializeTrayIcon();
    }

    // Fill in the balloon information
    g_nid.uFlags = NIF_INFO | NIF_ICON;
    wcsncpy_s(g_nid.szInfo, message.c_str(), _TRUNCATE);
    wcsncpy_s(g_nid.szInfoTitle, title.c_str(), _TRUNCATE);
    g_nid.dwInfoFlags = NIIF_INFO;

    // Display the balloon (non-blocking)
    if (!Shell_NotifyIconW(NIM_MODIFY, &g_nid)) {
        OutputDebugStringW(L"ShowBalloonNotification: Shell_NotifyIconW NIM_MODIFY failed\n");
    }
}

// Remove tray icon on exit
void RemoveTrayIcon()
{
    if (g_notifyIconAdded) {
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        g_notifyIconAdded = false;
    }
}

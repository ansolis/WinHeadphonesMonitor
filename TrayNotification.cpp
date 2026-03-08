/*
 * TrayNotification.cpp
 * 
 * MODULE: Tray Notification (System Tray UI)
 * PURPOSE: Implements system tray icon management and notification display
 * 
 * DESCRIPTION:
 *   Implements tray icon integration via Windows Shell API (Shell_NotifyIcon).
 *   Manages the small icon that appears in the system tray, balloon notifications,
 *   and tooltip text that displays current Bluetooth device status.
 * 
 * KEY OPERATIONS:
 *   - InitializeTrayIcon() - Add icon to tray with initial tooltip
 *   - ShowBalloonNotification() - Display popup balloon with message
 *   - UpdateTrayTooltip() - Refresh tooltip text (device status)
 *   - RemoveTrayIcon() - Remove icon from tray on exit
 *   - ProcessTrayMessage() - Handle tray icon callbacks
 * 
 * TRAY ICON LIFECYCLE:
 *   1. WinMain creates window and icon
 *   2. InitializeTrayIcon() adds icon to tray via Shell_NotifyIcon(NIM_ADD)
 *   3. Icon visible in tray, displays tooltips on hover
 *   4. Balloon notifications show via Shell_NotifyIcon(NIM_MODIFY) with NIIF flags
 *   5. RemoveTrayIcon() removes icon via Shell_NotifyIcon(NIM_DELETE) on exit
 * 
 * NOTIFYICONDATAW STRUCTURE:
 *   Contains:
 *   - hWnd: Window handle for callbacks
 *   - uID: Icon ID (unique identifier)
 *   - uFlags: What fields are valid (szTip, hIcon, uCallbackMessage, etc.)
 *   - uCallbackMessage: Custom message ID (WM_TRAYICON)
 *   - hIcon: Icon to display
 *   - szTip: Tooltip text (max 128 chars)
 *   - dwState: Icon state (NIS_HIDDEN, etc.)
 *   - szInfo: Balloon text
 *   - uTimeout: Balloon timeout (ms)
 *   - szInfoTitle: Balloon title
 *   - dwInfoFlags: Balloon type (NIIF_INFO, NIIF_WARNING, NIIF_ERROR)
 * 
 * MESSAGE CONSTANTS:
 *   - WM_TRAYICON: Custom message ID sent by OS on user interaction
 *   - IDM_EXIT: Menu command ID for application exit
 * 
 * BALLOON NOTIFICATIONS:
 *   Triggered via NotificationManager:
 *   - ShowBalloonNotification(title, text, timeout)
 *   - Updates tooltip simultaneously
 *   - Balloon displays for specified timeout (ms)
 *   - User can click balloon to activate window
 * 
 * TOOLTIP UPDATES:
 *   Called by NotificationManager on device status change:
 *   - UpdateTrayTooltip(status)
 *   - Text shown on mouse hover over tray icon
 *   - Max 128 characters (Windows limitation)
 *   - Examples:
 *     "WH-1000XM3 (ACTIVE OUTPUT)"
 *     "Sony Device (ACTIVE) + 1 other"
 *     "Bluetooth audio: disconnected"
 * 
 * EVENT ROUTING:
 *   Windows sends WM_TRAYICON when:
 *   - User left-clicks tray icon
 *   - User right-clicks tray icon
 *   - Balloon notification clicked
 *   - Mouse hovers over icon (for tooltip)
 *   
 *   ProcessTrayMessage() determines event type and routes appropriately
 * 
 * LOGGING:
 *   [TrayNotification] prefix on all debug output
 *   Examples:
 *     [TrayNotification] InitializeTrayIcon: hicon=0x12345678
 *     [TrayNotification] ShowBalloon: 'Connected WH-1000XM3' (2500ms)
 *     [TrayNotification] UpdateTooltip: 'WH-1000XM3 (ACTIVE OUTPUT)'
 *     [TrayNotification] RemoveTrayIcon: Complete
 * 
 * SHELL API CALLS:
 *   - Shell_NotifyIcon(NIM_ADD) - Add icon to tray
 *   - Shell_NotifyIcon(NIM_MODIFY) - Update icon properties
 *   - Shell_NotifyIcon(NIM_MODIFY + balloon) - Show notification
 *   - Shell_NotifyIcon(NIM_DELETE) - Remove icon from tray
 * 
 * ERROR HANDLING:
 *   - Shell_NotifyIcon returns BOOL (success/failure)
 *   - Failures logged but non-fatal (tray is optional feature)
 *   - Graceful degradation if tray unavailable
 * 
 * GLOBAL STATE:
 *   - NOTIFYICONDATAW structure initialized once
 *   - Icon handle stored for property updates
 *   - Window handle stored for callbacks
 *   - Tooltip text updated on each status change
 *   - Thread-safe for read-only access from main thread only
 */

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

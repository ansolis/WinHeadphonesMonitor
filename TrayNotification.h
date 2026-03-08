/*
 * TrayNotification.h
 * 
 * MODULE: Tray Notification (System Tray UI)
 * PURPOSE: Manage system tray icon, balloon notifications, and tooltip text
 * 
 * DESCRIPTION:
 *   Provides system tray icon integration for the Bluetooth headphones monitor.
 *   Manages tray icon display, balloon notification popups, and tooltip text that
 *   displays the current device status.
 * 
 *   Key responsibilities:
 *   - Initialize tray icon with application icon
 *   - Display balloon notifications (Stage 1/2/3/Removal)
 *   - Update tooltip text with device status
 *   - Handle tray icon click/right-click events
 *   - Clean up tray icon on application exit
 * 
 * USAGE:
 *   1. Initialize: InitializeTrayIcon(hwnd, hicon) - Add icon to tray
 *   2. Show balloons: ShowBalloonNotification(title, text, timeout)
 *   3. Update tooltip: UpdateTrayTooltip(text) - Set device status text
 *   4. Cleanup: RemoveTrayIcon() - Remove icon on exit
 * 
 * KEY FUNCTIONS:
 *   - InitializeTrayIcon() - Create tray icon with application icon
 *   - ShowBalloonNotification() - Display popup balloon at tray
 *   - UpdateTrayTooltip() - Update tooltip text (max 128 chars)
 *   - RemoveTrayIcon() - Remove icon from tray
 *   - ProcessTrayMessage() - Handle tray icon callbacks
 * 
 * FEATURES:
 *   - Balloon notifications with custom text and timeout
 *   - Tooltip synchronization with device status
 *   - Click/right-click event routing
 *   - Icon state updates (color change, animation possible)
 *   - Proper resource cleanup on exit
 * 
 * INTEGRATION:
 *   - Called by: NotificationManager for balloon display
 *   - Called by: WindowManager for tray event handling
 *   - Uses: Shell API (Shell_NotifyIcon) for tray interaction
 *   - Displays: Application icon and device notifications
 *   - Receives: Left-click (tooltip toggle), Right-click (context menu)
 * 
 * TRAY MESSAGES:
 *   - WM_TRAYICON: Custom message ID for tray callbacks
 *   - Sent by OS when user interacts with tray icon
 *   - Routed to WindowProc for processing
 * 
 * COMMAND IDs:
 *   - IDM_EXIT: User selected "Exit" from right-click menu
 * 
 * TOOLTIP LIMITS:
 *   - Maximum 128 characters (Windows API limitation)
 *   - Longer text is truncated
 *   - Should use smart formatting for multi-device scenarios
 *   - Examples:
 *     "WH-1000XM3 (ACTIVE OUTPUT)"
 *     "Sony Device (ACTIVE) + 1 other"
 *     "Bluetooth audio: disconnected"
 * 
 * NOTIFICATION TYPES:
 *   - NIIF_INFO: Information balloon (blue icon)
 *   - NIIF_WARNING: Warning balloon (yellow icon)
 *   - NIIF_ERROR: Error balloon (red icon)
 *   - Can be combined with timeout (typically 10 seconds)
 * 
 * LOGGING:
 *   All output prefixed with "[TrayNotification]" for easy filtering
 *   Examples:
 *     [TrayNotification] InitializeTrayIcon: Success
 *     [TrayNotification] ShowBalloon: 'Connected' at 2500ms
 *     [TrayNotification] UpdateTooltip: 'WH-1000XM3 (ACTIVE)'
 *     [TrayNotification] RemoveTrayIcon: Cleanup complete
 */

#pragma once

#include <windows.h>
#include <string>

// Tray callback message ID
extern const UINT WM_TRAYICON;

// Tray menu command IDs
extern const UINT_PTR IDM_EXIT;

// Initialize tray icon at startup (creates it if needed)
void InitializeTrayIcon();

// Initialize and display a balloon notification via system tray
// Shows both tray icon (if not already added) and balloon message
void ShowBalloonNotification(const std::wstring& title, const std::wstring& message);

// Get/set the window handle used for tray notifications
void SetTrayWindowHandle(HWND hwnd);
HWND GetTrayWindowHandle();

// Remove tray icon on exit
void RemoveTrayIcon();

// Update tray tooltip
void UpdateTrayTooltip(const std::wstring& tooltip);

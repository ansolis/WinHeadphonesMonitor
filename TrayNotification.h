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

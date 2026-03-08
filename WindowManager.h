#pragma once

#include <windows.h>

// Register the window class for device monitoring
// Returns true if successful
bool RegisterWindowClass(HINSTANCE hInstance);

// Create a hidden message-only window to receive device notifications
// Returns window handle or NULL on failure
HWND CreateDeviceMonitorWindow(HINSTANCE hInstance);

// Window procedure for handling device notifications and tray events
// Registered for the hidden window
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

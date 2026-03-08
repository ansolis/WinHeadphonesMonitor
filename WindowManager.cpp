#include "WindowManager.h"
#include "TrayNotification.h"
#include "BluetoothDeviceManager.h"
#include "DevicePropertyReader.h"
#include <dbt.h>

// Forward declarations for functions defined in main file
extern std::wstring GetHeadphonesBatteryLevel();
extern void HandleDeviceInterfaceArrival(PDEV_BROADCAST_DEVICEINTERFACE_W pDev);

// Register the window class for device monitoring
bool RegisterWindowClass(HINSTANCE hInstance)
{
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"HeadphonesBatteryMonitor";

    if (!RegisterClassW(&wc)) {
        return false;
    }
    return true;
}

// Create a hidden message-only window to receive device notifications
HWND CreateDeviceMonitorWindow(HINSTANCE hInstance)
{
    HWND hwnd = CreateWindowExW(
        0,
        L"HeadphonesBatteryMonitor",
        L"Battery Monitor",
        0,
        0, 0, 0, 0,
        HWND_MESSAGE,  // Message-only window (hidden)
        NULL,
        hInstance,
        NULL
    );

    if (!hwnd) {
        return NULL;
    }

    return hwnd;
}

// Global for debouncing devnodes-changed notifications
static ULONGLONG g_lastDevnodesNotify = 0;
static std::wstring g_lastNotifyMessage;
static ULONGLONG g_lastNotifyTime = 0;

// Helper to format device arrival message with device name
static std::wstring FormatDeviceArrivalMessage(const std::wstring& deviceName)
{
    std::wstring msg = L"[WindowProc] Device arrival detected for: '";
    msg += deviceName.empty() ? L"(unknown)" : deviceName;
    msg += L"'\n";
    return msg;
}

// Window procedure for handling device notifications and tray events
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    extern bool g_showNotifications;

    if (uMsg == WM_DEVICECHANGE) {
        if (!g_showNotifications) return TRUE;

        // Handle device interface arrivals specifically
        if (wParam == DBT_DEVICEARRIVAL) {
            OutputDebugStringW(L"WindowProc: WM_DEVICECHANGE - DBT_DEVICEARRIVAL\n");
            PDEV_BROADCAST_HDR pHdr = (PDEV_BROADCAST_HDR)lParam;
            if (pHdr && pHdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {
                HandleDeviceInterfaceArrival((PDEV_BROADCAST_DEVICEINTERFACE_W)pHdr);
            } else {
                OutputDebugStringW(L"WindowProc: DBT_DEVICEARRIVAL but no deviceinterface header\n");
            }
            return TRUE;
        }

        // Handle devnodes-changed messages
        if (wParam == DBT_DEVNODES_CHANGED) {
            OutputDebugStringW(L"WindowProc: WM_DEVICECHANGE - DBT_DEVNODES_CHANGED\n");

            // Throttle rapid devnode changes
            ULONGLONG now = GetTickCount64();
            if ((now - g_lastDevnodesNotify) < 1000) {
                OutputDebugStringW(L"WindowProc: devnodes change suppressed by throttle\n");
                return TRUE;
            }
            g_lastDevnodesNotify = now;

            // Small delay to let device stack settle
            Sleep(500);

            std::wstring batteryInfo = GetHeadphonesBatteryLevel();
            OutputDebugStringW(L"WindowProc: Check after DBT_DEVNODES_CHANGED returned:\n");
            OutputDebugStringW(batteryInfo.c_str());

            if (batteryInfo.find(L"WH-1000XM3") != std::wstring::npos) {
                if (batteryInfo == g_lastNotifyMessage && (now - g_lastNotifyTime) < 15000) {
                    OutputDebugStringW(L"WindowProc: duplicate notification suppressed (devnodes)\n");
                } else {
                    ShowBalloonNotification(L"Headphones Connected", batteryInfo);
                    g_lastNotifyTime = now;
                    g_lastNotifyMessage = batteryInfo;
                }
            }
            return TRUE;
        }

        return TRUE;
    }
    else if (uMsg == WM_TRAYICON) {
        // Handle tray icon events (right-click to show context menu)
        if (lParam == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            if (hMenu) {
                InsertMenuW(hMenu, 0, MF_BYPOSITION | MF_STRING, IDM_EXIT, L"Exit");
                // Set foreground window before TrackPopupMenu to ensure menu closes correctly
                SetForegroundWindow(hwnd);
                TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
                DestroyMenu(hMenu);
            }
        }

        // Left-click on the tray icon: show current battery level (modal)
        if (lParam == WM_LBUTTONUP) {
            OutputDebugStringW(L"WindowProc: WM_TRAYICON - WM_LBUTTONUP (query battery)\n");

            bool connected = IsAnyHeadsetConnected();
            std::wstring batteryInfo = GetHeadphonesBatteryLevel();
            bool hasBattery = (batteryInfo.find(L"Battery Level:") != std::wstring::npos);

            if (connected) {
                if (hasBattery) {
                    MessageBoxW(hwnd, batteryInfo.c_str(), L"Headphones Battery", MB_OK | MB_ICONINFORMATION);
                } else {
                    MessageBoxW(hwnd, L"WH-1000XM3 connected but battery level not available", L"Headphones Battery", MB_OK | MB_ICONWARNING);
                }
            } else {
                if (hasBattery) {
                    std::wstring msg = batteryInfo;
                    msg += L"\n\nNote: the headset is currently disconnected - this is the last-seen battery level.";
                    MessageBoxW(hwnd, msg.c_str(), L"Headphones Battery (last seen)", MB_OK | MB_ICONINFORMATION);
                } else {
                    MessageBoxW(hwnd, L"WH-1000XM3 not connected or battery not available", L"Headphones Battery", MB_OK | MB_ICONWARNING);
                }
            }
        }
        return TRUE;
    }
    else if (uMsg == WM_COMMAND) {
        if (LOWORD(wParam) == IDM_EXIT) {
            OutputDebugStringW(L"Tray menu: Exit selected\n");
            RemoveTrayIcon();
            PostQuitMessage(0);
            return 0;
        }
    }
    else if (uMsg == WM_DESTROY) {
        // Ensure tray icon is removed on destroy
        RemoveTrayIcon();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

/*
 * WindowManager.cpp
 * 
 * MODULE: Window Manager (Message Handling & Device Notifications)
 * PURPOSE: Implements message-only window for device and tray notification handling
 * 
 * DESCRIPTION:
 *   Implements the core message-only window infrastructure that receives all device
 *   and user interface notifications. Registers window class, creates invisible window,
 *   and dispatches messages to appropriate handlers.
 * 
 * WINDOW CLASS REGISTRATION:
 *   1. Create WNDCLASSEXW structure
 *   2. Set WindowProc callback
 *   3. Set window class name
 *   4. Register with RegisterClassExW()
 *   5. Save class name for CreateWindowEx()
 * 
 * WINDOW CREATION:
 *   1. Create message-only window with CreateWindowExW()
 *   2. Parent: HWND_MESSAGE (special constant for message-only windows)
 *   3. No coordinates (message-only windows are invisible)
 *   4. Store window handle for device notification registration
 *   5. Return HWND to caller
 * 
 * DEVICE NOTIFICATION SETUP:
 *   1. After window creation, call RegisterDeviceNotification()
 *   2. Register for DBT_DEVTYP_DEVICEINTERFACE
 *   3. Subscribe to GUID_DEVINTERFACE_BLUETOOTH_RADIO
 *   4. Window receives WM_DEVICECHANGE for all Bluetooth events
 * 
 * WINDOW PROCEDURE (WindowProc):
 *   Main message dispatch handler:
 *   - WM_DEVICECHANGE: Route to HandleDeviceInterfaceArrival()
 *   - WM_TRAYICON: Route to DeviceContextMenu->ShowMenu() or handle
 *   - WM_DESTROY: Clean up and signal exit
 *   - Default: DefWindowProcW() for unhandled messages
 * 
 * MESSAGE DISPATCH:
 *   WM_DEVICECHANGE handling:
 *   1. Extract DEV_BROADCAST_DEVICEINTERFACE_W from lParam
 *   2. Check event type (DBT_DEVICEARRIVAL, DBT_DEVICEREMOVE*)
 *   3. Extract device interface GUID
 *   4. Call HandleDeviceInterfaceArrival() for new devices
 *   5. Update DeviceRegistry for removals
 * 
 *   WM_TRAYICON handling:
 *   1. Determine event type (left-click, right-click, balloon click)
 *   2. Get current mouse position if needed
 *   3. Call appropriate handler (ShowMenu, etc.)
 * 
 * DEVICE INTERFACE EXTRACTION:
 *   DEV_BROADCAST_DEVICEINTERFACE_W contains:
 *   - dbch_size: Structure size
 *   - dbch_devicetype: Must be DBT_DEVTYP_DEVICEINTERFACE
 *   - dbch_reserved: Reserved field
 *   - dbcl_classगuid: Device class GUID (e.g., BLUETOOTH_RADIO)
 *   - dbcc_name: Device name/path
 * 
 * LOGGING:
 *   [WindowManager] prefix on all debug output
 *   Examples:
 *     [WindowManager] RegisterWindowClass: Class registered
 *     [WindowManager] CreateDeviceMonitorWindow: HWND=0x00020456
 *     [WindowManager] WindowProc: WM_DEVICECHANGE (DBT_DEVICEARRIVAL)
 *     [WindowManager] Device interface arrival detected
 *     [WindowManager] WM_TRAYICON: Right-click at 640,480
 * 
 * FUNCTION CALLS:
 *   - RegisterClassExW() - Register window class
 *   - CreateWindowExW() - Create message-only window
 *   - RegisterDeviceNotificationW() - Subscribe to device events
 *   - UnregisterDeviceNotification() - Cleanup (on exit)
 *   - DefWindowProcW() - Default message handling
 * 
 * FORWARD DECLARATIONS:
 *   - HandleDeviceInterfaceArrival() - Defined in HeadphonesBattery.cpp (main)
 *   - GetHeadphonesBatteryLevel() - Defined in HeadphonesBattery.cpp (main)
 * 
 * INTEGRATION:
 *   - Called by: WinMain for window creation
 *   - Calls: HandleDeviceInterfaceArrival() on device arrival
 *   - Calls: DeviceContextMenu->ShowMenu() on tray right-click
 *   - Receives: OS device notifications (WM_DEVICECHANGE)
 *   - Receives: User tray notifications (WM_TRAYICON)
 * 
 * THREAD SAFETY:
 *   - Message-only window processes on main thread
 *   - All handlers run on main thread context
 *   - Device notifications are OS-managed (thread-safe)
 *   - Window handle safe for cross-thread access (read-only)
 * 
 * CLEANUP:
 *   On WM_DESTROY or application exit:
 *   1. Call UnregisterDeviceNotificationW() to stop notifications
 *   2. Call DestroyWindow() to remove window
 *   3. Call UnregisterClassW() to unregister class
 *   4. PostQuitMessage() to exit message loop
 */

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

#include <windows.h>
#include <setupapi.h>
#include <devpkey.h>
#include <cfgmgr32.h>
#include <dbt.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <string>
#include <sstream>
#include <shellapi.h>
#include <vector>
#include <set>
#include <map>
#include <bluetoothapis.h>
#include <algorithm>
#include <cwctype>

// Module headers
#include "BluetoothDeviceManager.h"
#include "DevicePropertyReader.h"
#include "TrayNotification.h"
#include "WindowManager.h"
#include "AudioEndpointManager.h"
#include "DeviceRegistry.h"
#include "NotificationManager.h"
#include "Stage2Verifier.h"
#include "DeviceContextMenu.h"

#pragma comment(lib, "Bthprops.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "mmdevapi.lib")

// Forward declarations
void CheckAndNotifyNow();
void HandleDeviceInterfaceArrival(PDEV_BROADCAST_DEVICEINTERFACE_W pDev);

// Track currently-known connected device instance IDs to avoid false arrival on disconnect
// Map from root devinstance ID -> representative virtual device name
std::map<std::wstring, std::wstring> g_connectedDevices;

// Global variable to track if we should show notifications
bool g_showNotifications = false;

// Debounce state for arrival notifications
static ULONGLONG g_lastNotifyTime = 0;
static std::wstring g_lastNotifyMessage;

// Implement InitializeConnectedDevices using refactored Bluetooth functions

// Initialize and populate g_connectedDevices with currently-present WH-1000XM3 devices
void InitializeConnectedDevices()
{
    HDEVINFO deviceInfoSet = SetupDiGetClassDevsW(NULL, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        OutputDebugStringW(L"InitializeConnectedDevices: SetupDiGetClassDevsW failed\n");
        return;
    }

    SP_DEVINFO_DATA devInfo = {};
    devInfo.cbSize = sizeof(devInfo);
    DWORD idx = 0;

    while (SetupDiEnumDeviceInfo(deviceInfoSet, idx, &devInfo)) {
        idx++;

        WCHAR friendlyName[256] = { 0 };
        DWORD reqSize = 0;
        if (!SetupDiGetDeviceRegistryPropertyW(deviceInfoSet, &devInfo, SPDRP_FRIENDLYNAME, NULL, (PBYTE)friendlyName, sizeof(friendlyName), &reqSize)) {
            WCHAR deviceDesc[256] = { 0 };
            if (!SetupDiGetDeviceRegistryPropertyW(deviceInfoSet, &devInfo, SPDRP_DEVICEDESC, NULL, (PBYTE)deviceDesc, sizeof(deviceDesc), &reqSize)) {
                continue;
            }
            wcscpy_s(friendlyName, _countof(friendlyName), deviceDesc);
        }

        if (wcsstr(friendlyName, L"WH-1000XM3") == NULL) continue;

        // Only consider started devnodes as connected
        ULONG status = 0;
        ULONG problem = 0;
        CONFIGRET st = CM_Get_DevNode_Status(&status, &problem, devInfo.DevInst, 0);
        if (st != CR_SUCCESS) continue;
        if (!(status & DN_STARTED)) continue;

        DEVINST root = devInfo.DevInst;
        DEVINST parent = 0;
        while (CM_Get_Parent(&parent, root, 0) == CR_SUCCESS && parent != 0 && parent != root) {
            root = parent;
        }

        WCHAR rootIdBuf[512] = { 0 };
        if (CM_Get_Device_IDW(root, rootIdBuf, _countof(rootIdBuf), 0) != CR_SUCCESS) continue;
        std::wstring rootId = rootIdBuf;

        // Parse BT address and check if actually connected
        BLUETOOTH_ADDRESS btAddr = { 0 };
        if (!ParseBluetoothAddress(rootId, btAddr)) {
            std::wstring dbg = L"InitializeConnectedDevices: failed to parse BT address from ";
            dbg += rootId;
            dbg += L"\n";
            OutputDebugStringW(dbg.c_str());
            continue;
        }

        // Check Bluetooth connection state
        if (!IsBluetoothDeviceConnected(btAddr)) {
            std::wstring dbg = L"InitializeConnectedDevices: skipping not-connected root ";
            dbg += rootId;
            dbg += L"\n";
            OutputDebugStringW(dbg.c_str());
            continue;
        }

        g_connectedDevices[rootId] = std::wstring(friendlyName);
        std::wstring dbg = L"InitializeConnectedDevices: registered connected device ";
        dbg += friendlyName;
        dbg += L" (";
        // print parsed address
        wchar_t addrBuf[64];
        _snwprintf_s(addrBuf, _countof(addrBuf), _TRUNCATE, L"%02X:%02X:%02X:%02X:%02X:%02X", 
            btAddr.rgBytes[5], btAddr.rgBytes[4], btAddr.rgBytes[3], btAddr.rgBytes[2], btAddr.rgBytes[1], btAddr.rgBytes[0]);
        dbg += addrBuf;
        dbg += L")\n";
        OutputDebugStringW(dbg.c_str());
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);
}

// CheckAndNotifyNow: Check battery and show startup notification if connected
void CheckAndNotifyNow()
{
    std::wstring batteryInfo = GetHeadphonesBatteryLevel();
    OutputDebugStringW(L"CheckAndNotifyNow: battery info retrieved:\n");
    OutputDebugStringW(batteryInfo.c_str());

    // Only show at-startup notification if the headset is actually connected
    if (batteryInfo.find(L"WH-1000XM3") != std::wstring::npos) {
        if (IsAnyHeadsetConnected()) {
            ShowBalloonNotification(L"Headphones Connected", batteryInfo);
        } else {
            OutputDebugStringW(L"CheckAndNotifyNow: headset not connected, skipping startup balloon\n");
        }
    }
}

// Helper thread to re-check device connections after a delay
static DWORD WINAPI ArrivalRecheckThread(LPVOID) {
    Sleep(500);  // wait for stack to settle
    size_t before = g_connectedDevices.size();
    InitializeConnectedDevices();
    size_t after = g_connectedDevices.size();
    if (after > before) {
        std::wstring batteryInfo = GetHeadphonesBatteryLevel();
        OutputDebugStringW(L"Recheck thread: found new connected device(s)\n");
        OutputDebugStringW(batteryInfo.c_str());
        if (batteryInfo.find(L"WH-1000XM3") != std::wstring::npos) {
            ShowBalloonNotification(L"Headphones Connected", batteryInfo);
            UpdateTrayTooltip(L"Headphones connected");
        }
    }
    return 0;
}

// Process additions detected in device arrival
static void ProcessDeviceAdditions(const std::map<std::wstring, std::pair<std::wstring, std::wstring>>& current, ULONGLONG now)
{
    std::set<std::wstring> currentIds;
    for (const auto& p : current) currentIds.insert(p.first);

    for (const auto& id : currentIds) {
        if (g_connectedDevices.find(id) == g_connectedDevices.end()) {
            // New device connected
            const auto& entry = current.at(id);
            OutputDebugStringW(L"HandleDeviceInterfaceArrival: new device detected, showing balloon\n");
            {
                std::wstring vdbg = entry.first + std::wstring(L"\n");
                OutputDebugStringW(vdbg.c_str());
            }
            ShowBalloonNotification(L"Headphones Connected", entry.second);
            g_lastNotifyTime = now;
            g_lastNotifyMessage = entry.second;
            UpdateTrayTooltip(L"Headphones connected");
        }
    }
}

// Process removals detected in device arrival
static void ProcessDeviceRemovals(const std::map<std::wstring, std::pair<std::wstring, std::wstring>>& current)
{
    std::set<std::wstring> currentIds;
    for (const auto& p : current) currentIds.insert(p.first);

    for (auto it = g_connectedDevices.begin(); it != g_connectedDevices.end();) {
        if (currentIds.find(it->first) == currentIds.end()) {
            // Device removed
            OutputDebugStringW(L"HandleDeviceInterfaceArrival: device removal detected\n");
            {
                std::wstring vdbg = it->second + std::wstring(L"\n");
                OutputDebugStringW(vdbg.c_str());
            }
            ShowBalloonNotification(L"Headphones Disconnected", L"Headphones disconnected");
            it = g_connectedDevices.erase(it);
        } else {
            ++it;
        }
    }

    // Update tooltip if no devices remain
    if (g_connectedDevices.empty()) {
        UpdateTrayTooltip(L"Headphones disconnected");
    }
}

void HandleDeviceInterfaceArrival(PDEV_BROADCAST_DEVICEINTERFACE_W pDev) {
    // Simpler approach: enumerate present devnodes and find the WH-1000XM3 entry.
    // This avoids relying on the notification's class GUID (which can be GUID_NULL).
    if (!pDev || !pDev->dbcc_name) return;

    OutputDebugStringW(L"[HandleDeviceInterfaceArrival] Received interface notification\n");
    // Debug: show the incoming device interface path (may help identify which interface fired)
    if (pDev->dbcc_name) {
        std::wstring dbgPath = L"[HandleDeviceInterfaceArrival] Interface path=";
        dbgPath += pDev->dbcc_name;
        dbgPath += L"\n";
        OutputDebugStringW(dbgPath.c_str());

        // Check if it's a stage 2 (MMDevice) arrival
        if (wcsstr(pDev->dbcc_name, L"SWD#MMDEVAPI")) {
            OutputDebugStringW(L"[HandleDeviceInterfaceArrival] STAGE 2 MARKER DETECTED: MMDevice endpoint arrival\n");
        } else if (wcsstr(pDev->dbcc_name, L"INTELAUDIO") || wcsstr(pDev->dbcc_name, L"IntcBtWav")) {
            OutputDebugStringW(L"[HandleDeviceInterfaceArrival] STAGE 1 MARKER: Driver interface arrival\n");
        }
    }
    ULONGLONG now = GetTickCount64();

    // Debounced async re-check: some stacks deliver many interface notifications
    // Start a background thread to wait and re-check after stack settles
    static ULONGLONG s_lastRecheck = 0;
    if ((now - s_lastRecheck) > 1000) {
        s_lastRecheck = now;
        HANDLE h = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ArrivalRecheckThread, NULL, 0, NULL);
        if (h) CloseHandle(h);
    }

    // Refresh connected devices list and process additions/removals
    size_t oldConnectedCount = g_connectedDevices.size();
    InitializeConnectedDevices();

    if (g_connectedDevices.size() > oldConnectedCount) {
        // New device(s) connected
        std::wstring batteryInfo = GetHeadphonesBatteryLevel();

        wchar_t logMsg[256];
        _snwprintf_s(logMsg, _countof(logMsg), _TRUNCATE, 
            L"[HandleDeviceInterfaceArrival] New device(s) detected. Old count=%zu, New count=%zu\n",
            oldConnectedCount, g_connectedDevices.size());
        OutputDebugStringW(logMsg);
        OutputDebugStringW(batteryInfo.c_str());

        if (batteryInfo.find(L"WH-1000XM3") != std::wstring::npos) {
            ShowBalloonNotification(L"Headphones Connected", batteryInfo);
            g_lastNotifyTime = now;
            g_lastNotifyMessage = batteryInfo;
        }
        UpdateTrayTooltip(L"Headphones connected");
        return;
    }

    // Check for removals (devices in known map but not in current enumeration)
    std::set<std::wstring> current;
    for (const auto& device : g_connectedDevices) {
        current.insert(device.first);
    }

    // If we have fewer devices than before, notify of removal
    for (auto it = g_connectedDevices.begin(); it != g_connectedDevices.end(); ++it) {
        if (current.find(it->first) == current.end()) {
            // Device was removed
            wchar_t logMsg[512];
            _snwprintf_s(logMsg, _countof(logMsg), _TRUNCATE,
                L"[HandleDeviceInterfaceArrival] Device removal detected: '%s'\n",
                it->second.c_str());
            OutputDebugStringW(logMsg);

            ShowBalloonNotification(L"Headphones Disconnected", L"Headphones disconnected");
            g_connectedDevices.erase(it);
            if (g_connectedDevices.empty()) {
                UpdateTrayTooltip(L"Headphones disconnected");
            }
            return;
        }
    }
}

// Main entry point: set up window, initialize devices, and run message loop

// Main entry point: set up window, initialize devices, and run message loop
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    OutputDebugStringW(L"[WinMain] HeadphonesBattery starting...\n");

    // Initialize COM for MMDevice and Windows multimedia APIs
    HRESULT hrCOM = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hrCOM)) {
        OutputDebugStringW(L"[WinMain] CoInitializeEx failed\n");
        return 1;
    }
    OutputDebugStringW(L"[WinMain] COM initialized successfully\n");

    // Initialize registry managers
    g_deviceRegistry = new DeviceRegistry();
    g_notificationManager = new NotificationManager();
    g_stage2Verifier = new Stage2Verifier();
    g_deviceContextMenu = new DeviceContextMenu();

    // Enable notifications after initialization
    g_showNotifications = true;

    // Register window class for device monitoring
    if (!RegisterWindowClass(hInstance)) {
        OutputDebugStringW(L"[WinMain] Failed to register window class\n");
        CoUninitialize();
        return 1;
    }

    // Create hidden window to receive device notifications
    HWND hwnd = CreateDeviceMonitorWindow(hInstance);
    if (!hwnd) {
        OutputDebugStringW(L"[WinMain] Failed to create device monitor window\n");
        CoUninitialize();
        return 1;
    }

    // Set window handle for tray notifications
    SetTrayWindowHandle(hwnd);

    // Initialize tray icon at startup
    InitializeTrayIcon();

    // Initialize AudioEndpointManager for MMDevice notifications
    g_audioEndpointManager = new AudioEndpointManager();
    if (!g_audioEndpointManager->Initialize()) {
        OutputDebugStringW(L"[WinMain] Failed to initialize AudioEndpointManager\n");
    }

    // Enumerate Bluetooth audio devices (populates g_bluetoothAudioDevices registry)
    OutputDebugStringW(L"[WinMain] Enumerating Bluetooth audio devices...\n");
    int audioDeviceCount = EnumerateBluetoothAudioDevices();
    wchar_t msg[256];
    _snwprintf_s(msg, _countof(msg), _TRUNCATE, L"[WinMain] Found %d connected audio devices\n", audioDeviceCount);
    OutputDebugStringW(msg);

    // Initialize connected devices map from current system state
    InitializeConnectedDevices();

    // Populate device registry from Bluetooth enumeration
    for (const auto& pair : g_bluetoothAudioDevices) {
        BluetoothAudioDevice device;
        device.bluetoothAddress = pair.second.bluetoothAddress;
        device.friendlyName = pair.second.friendlyName;
        device.btAddr = pair.second.btAddr;
        device.isConnected = pair.second.isConnected;
        device.isDefaultOutput = false;
        device.batteryLevel = 0;
        device.lastStateChangeTime = GetTickCount64();

        g_deviceRegistry->AddOrUpdateDevice(device);
    }

    // Check and notify if headphones are already connected
    CheckAndNotifyNow();

    // Update tooltip based on registry state
    g_notificationManager->UpdateTrayTooltip();

    // Register for all device interface notifications
    DEV_BROADCAST_DEVICEINTERFACE_W notificationFilter = {};
    notificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE_W);
    notificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;

    HDEVNOTIFY hDevNotify = RegisterDeviceNotificationW(
        hwnd,
        &notificationFilter,
        DEVICE_NOTIFY_WINDOW_HANDLE | DEVICE_NOTIFY_ALL_INTERFACE_CLASSES
    );

    if (!hDevNotify) {
        OutputDebugStringW(L"[WinMain] Failed to register device notification\n");
        RemoveTrayIcon();
        if (g_audioEndpointManager) {
            g_audioEndpointManager->Shutdown();
            delete g_audioEndpointManager;
            g_audioEndpointManager = nullptr;
        }
        if (g_deviceRegistry) delete g_deviceRegistry;
        if (g_notificationManager) delete g_notificationManager;
        if (g_stage2Verifier) delete g_stage2Verifier;
        if (g_deviceContextMenu) delete g_deviceContextMenu;
        DestroyWindow(hwnd);
        CoUninitialize();
        return 1;
    }

    OutputDebugStringW(L"[WinMain] Initialization complete, entering message loop\n");

    // Message loop - keeps the application running
    MSG msg_data = {};
    while (GetMessage(&msg_data, NULL, 0, 0)) {
        TranslateMessage(&msg_data);
        DispatchMessage(&msg_data);
    }

    // Cleanup
    OutputDebugStringW(L"[WinMain] Cleaning up: unregistering and removing tray icon\n");
    UnregisterDeviceNotification(hDevNotify);
    RemoveTrayIcon();

    if (g_audioEndpointManager) {
        g_audioEndpointManager->Shutdown();
        delete g_audioEndpointManager;
        g_audioEndpointManager = nullptr;
    }

    if (g_deviceRegistry) delete g_deviceRegistry;
    if (g_notificationManager) delete g_notificationManager;
    if (g_stage2Verifier) delete g_stage2Verifier;
    if (g_deviceContextMenu) delete g_deviceContextMenu;

    DestroyWindow(hwnd);

    CoUninitialize();
    OutputDebugStringW(L"[WinMain] HeadphonesBattery exiting\n");

    return 0;
}
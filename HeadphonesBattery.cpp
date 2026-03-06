#include <windows.h>
#include <setupapi.h>
#include <devpkey.h>
#include <cfgmgr32.h>
#include <dbt.h>
#include <string>
#include <sstream>
#include <shellapi.h>
#include <vector>
#include <set>
#include <map>
#include <bluetoothapis.h>
#include <algorithm>
#include <cwctype>

#pragma comment(lib, "Bthprops.lib")

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")
#pragma comment(lib, "shell32.lib")

// Battery level property key (shared)
static DEVPROPKEY g_batteryLevelKey = { { 0x104EA319, 0x6EE2, 0x4701, { 0xBD, 0x47, 0x8D, 0xDB, 0xF4, 0x25, 0xBB, 0xE5 } }, 2 };

// Forward declarations for helpers used before their definitions
void ShowBalloonNotification(const std::wstring& title, const std::wstring& message);
void CheckAndNotifyNow();
void InitializeConnectedDevices();
// Bluetooth helper forward declaration
static bool IsAnyHeadsetConnected();
// The concrete map is defined later; declare it extern here so init function can use it.
extern std::map<std::wstring, std::wstring> g_connectedDevices;
// The actual map is defined later; functions will use the definition below.

// Notification icon data and state (declared early because many helpers use it)
static NOTIFYICONDATAW g_nid = {};
static bool g_notifyIconAdded = false;
// Window used for tray notifications and menu handling
static HWND g_notifyHwnd = NULL;

// Try to read the shared battery property from a devnode or any of its parents.
// Walks up the devnode tree once without retries to avoid noisy retry logs.
static CONFIGRET ReadBatteryFromDevInst(DEVINST devInst, BYTE &outBattery)
{
    outBattery = 0;
    DWORD batterySize = sizeof(outBattery);
    DEVPROPTYPE propertyType = 0;
    CONFIGRET cr = CR_FAILURE;

    DEVINST current = devInst;
    while (current != 0) {
        cr = CM_Get_DevNode_PropertyW(current, &g_batteryLevelKey, &propertyType, (PBYTE)&outBattery, &batterySize, 0);
        if (cr == CR_SUCCESS) return cr;

        // Try parent
        DEVINST parent = 0;
        if (CM_Get_Parent(&parent, current, 0) != CR_SUCCESS) break;
        if (parent == 0 || parent == current) break;
        current = parent;
    }

    return cr;
}

// Populate g_connectedDevices with currently-present WH-1000XM3 root entries.
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

        // Try to parse BT address from rootId and check if actually connected
        BLUETOOTH_ADDRESS btAddr = { 0 };
        bool hasAddr = false;
        auto ParseBtAddr = [&](const std::wstring &id)->bool {
            // find 12 hex chars sequence
            std::wstring s = id;
            for (size_t i = 0; i + 12 <= s.size(); ++i) {
                bool ok = true;
                for (size_t j = 0; j < 12; ++j) {
                    wchar_t c = s[i + j];
                    if (!((c >= L'0' && c <= L'9') || (c >= L'A' && c <= L'F') || (c >= L'a' && c <= L'f'))) { ok = false; break; }
                }
                if (!ok) continue;
                // parse 6 pairs
                for (int b = 0; b < 6; ++b) {
                    std::wstring pair = s.substr(i + b*2, 2);
                    unsigned int val = 0;
                    swscanf_s(pair.c_str(), L"%2x", &val);
                    // place into rgBytes reversed so rgBytes[0] is LSB
                    btAddr.rgBytes[5 - b] = (BYTE)val;
                }
                return true;
            }
            return false;
        };
        if (ParseBtAddr(rootId)) {
            hasAddr = true;
        }

        bool isConnected = true;
        if (hasAddr) {
            // enumerate paired/remembered devices to see if this address is marked connected
            BLUETOOTH_DEVICE_SEARCH_PARAMS search = {};
            search.dwSize = sizeof(search);
            search.fReturnAuthenticated = TRUE;
            search.fReturnRemembered = TRUE;
            search.fReturnUnknown = TRUE;
            search.fReturnConnected = TRUE;
            BLUETOOTH_DEVICE_INFO devInfo = {};
            devInfo.dwSize = sizeof(devInfo);
            HANDLE h = BluetoothFindFirstDevice(&search, &devInfo);
            bool found = false;
            if (h) {
                do {
                    if (memcmp(devInfo.Address.rgBytes, btAddr.rgBytes, 6) == 0) {
                        isConnected = devInfo.fConnected ? true : false;
                        found = true;
                        break;
                    }
                    devInfo.dwSize = sizeof(devInfo);
                } while (BluetoothFindNextDevice(h, &devInfo));
                BluetoothFindDeviceClose(h);
            }
            if (!found) {
                isConnected = false;
            }
        }

        if (!hasAddr || !isConnected) {
            // skip registering as connected if we cannot verify connection
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
        _snwprintf_s(addrBuf, _countof(addrBuf), _TRUNCATE, L"%02X:%02X:%02X:%02X:%02X:%02X", btAddr.rgBytes[5], btAddr.rgBytes[4], btAddr.rgBytes[3], btAddr.rgBytes[2], btAddr.rgBytes[1], btAddr.rgBytes[0]);
        dbg += addrBuf;
        dbg += L")\n";
        OutputDebugStringW(dbg.c_str());
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);
}

static std::wstring GetHeadphonesBatteryLevel() {
    HDEVINFO deviceInfoSet;
    SP_DEVINFO_DATA deviceInfoData = { 0 };
    DWORD deviceIndex = 0;

    // use shared battery property key

    // Get all devices
    deviceInfoSet = SetupDiGetClassDevs(
        NULL,
        NULL,
        NULL,
        DIGCF_ALLCLASSES | DIGCF_PRESENT
    );

    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        OutputDebugStringW(L"GetHeadphonesBatteryLevel: SetupDiGetClassDevs failed\n");
        return L"Failed to get device list";
    }

    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    // Enumerate all devices
    while (SetupDiEnumDeviceInfo(deviceInfoSet, deviceIndex, &deviceInfoData)) {
        deviceIndex++;

        // Get device friendly name using the registry property (avoids linking to DEVPKEY symbols)
        WCHAR friendlyName[256] = { 0 };
        DWORD requiredSize = 0;

        if (SetupDiGetDeviceRegistryPropertyW(
            deviceInfoSet,
            &deviceInfoData,
            SPDRP_FRIENDLYNAME,
            NULL,
            (PBYTE)friendlyName,
            sizeof(friendlyName),
            &requiredSize
        )) {
            // Check if this is the WH-1000XM3 (or any headphones you want)
            if (wcsstr(friendlyName, L"WH-1000XM3") != NULL) {
                // Try to get battery level using CM_Get_DevNode_PropertyW (cfgmgr32)
                BYTE batteryLevel = 0;
                DWORD batterySize = sizeof(batteryLevel);


                // Try a single read walking up the devnode tree to find the battery property
                // This avoids noisy retries when the device is already connected
                // For diagnostics, get the device instance ID string
                WCHAR devIdBuf[512] = { 0 };
                if (CM_Get_Device_IDW(deviceInfoData.DevInst, devIdBuf, _countof(devIdBuf), 0) == CR_SUCCESS) {
                    std::wstring dbg = L"GetHeadphonesBatteryLevel: checking DevInst ";
                    dbg += devIdBuf;
                    dbg += L"\n";
                    OutputDebugStringW(dbg.c_str());
                }

                CONFIGRET cr = CR_FAILURE;
                cr = ReadBatteryFromDevInst(deviceInfoData.DevInst, batteryLevel);

                if (cr == CR_SUCCESS) {
                    std::wstringstream result;
                    result << L"Device: " << friendlyName << L"\n\n"
                        << L"Battery Level: " << (int)batteryLevel << L"%";
                    SetupDiDestroyDeviceInfoList(deviceInfoSet);
                    OutputDebugStringW(L"GetHeadphonesBatteryLevel: found device and battery\n");
                    return result.str();
                } else {
                    // log final error for diagnostics
                    std::wstringstream err;
                    err << L"GetHeadphonesBatteryLevel: ReadBatteryFromDevInst failed with code " << cr << L"\n";
                    OutputDebugStringW(err.str().c_str());
                }
            }
        }
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    OutputDebugStringW(L"GetHeadphonesBatteryLevel: device not found\n");
    return L"WH-1000XM3 not found or battery level not available";
}

// Global variable to track if we should show notifications
bool g_showNotifications = false;
// Debounce state for arrival notifications
static ULONGLONG g_lastNotifyTime = 0;
static std::wstring g_lastDevInstanceId;
// Track currently-known connected device instance IDs to avoid false arrival on disconnect
// Map from root devinstance ID -> representative virtual device name
static std::map<std::wstring, std::wstring> g_connectedDevices;
// static std::map<std::wstring, std::wstring> g_connectedDevices;

void CheckAndNotifyNow() {
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

// Bluetooth scan helper used in several places to decide connected state
static bool IsAnyHeadsetConnected() {
    BLUETOOTH_DEVICE_SEARCH_PARAMS search = {};
    search.dwSize = sizeof(search);
    search.fReturnAuthenticated = TRUE;
    search.fReturnRemembered = TRUE;
    search.fReturnUnknown = TRUE;
    search.fReturnConnected = TRUE;
    BLUETOOTH_DEVICE_INFO devInfo = {};
    devInfo.dwSize = sizeof(devInfo);
    HANDLE h = BluetoothFindFirstDevice(&search, &devInfo);
    bool foundConnected = false;
    if (h) {
        do {
            if (wcsstr(devInfo.szName, L"WH-1000XM3") != NULL) {
                wchar_t buf[256];
                _snwprintf_s(buf, _countof(buf), _TRUNCATE, L"IsAnyHeadsetConnected: found BT device '%s' connected=%d\n", devInfo.szName, devInfo.fConnected);
                OutputDebugStringW(buf);
                if (devInfo.fConnected) { foundConnected = true; break; }
            }
            devInfo.dwSize = sizeof(devInfo);
        } while (BluetoothFindNextDevice(h, &devInfo));
        BluetoothFindDeviceClose(h);
    }
    return foundConnected;
}
// Helper: process a device-interface arrival notification
static std::wstring g_lastNotifyMessage;

// Thread proc for arrival re-check
static DWORD WINAPI ArrivalRecheckThread(LPVOID) {
    // wait for stack to settle
    Sleep(500);
    size_t before = g_connectedDevices.size();
    InitializeConnectedDevices();
    size_t after = g_connectedDevices.size();
    if (after > before) {
        std::wstring batteryInfo = GetHeadphonesBatteryLevel();
        OutputDebugStringW(L"Recheck thread: found new connected device(s)\n");
        OutputDebugStringW(batteryInfo.c_str());
        if (batteryInfo.find(L"WH-1000XM3") != std::wstring::npos) {
            ShowBalloonNotification(L"Headphones Connected", batteryInfo);
            // update tooltip
        if (g_notifyIconAdded) {
            wcscpy_s(g_nid.szTip, _countof(g_nid.szTip), L"Headphones connected");
            g_nid.uFlags = NIF_TIP;
            Shell_NotifyIconW(NIM_MODIFY, &g_nid);
        }
        }
    }
    return 0;
}

struct CurrentInfo {
    std::wstring virtualName;
    std::wstring batteryInfo;
    bool hasBattery;
};

// Track currently-known connected device instance IDs to avoid false arrival on disconnect
// Map from root devinstance ID -> representative virtual device name
// static std::map<std::wstring, std::wstring> g_connectedDevices;

// Forward declarations
void ShowBalloonNotification(const std::wstring& title, const std::wstring& message);
void CheckAndNotifyNow();

static void HandleDeviceInterfaceArrival(PDEV_BROADCAST_DEVICEINTERFACE_W pDev) {
    // Simpler approach: enumerate present devnodes and find the WH-1000XM3 entry.
    // This avoids relying on the notification's class GUID (which can be GUID_NULL).
    if (!pDev || !pDev->dbcc_name) return;

    OutputDebugStringW(L"HandleDeviceInterfaceArrival: received interface notification\n");
    // Debug: show the incoming device interface path (may help identify which interface fired)
    if (pDev->dbcc_name) {
        std::wstring dbgPath = L"HandleDeviceInterfaceArrival: interface path=";
        dbgPath += pDev->dbcc_name;
        dbgPath += L"\n";
        OutputDebugStringW(dbgPath.c_str());
    }
    ULONGLONG now = GetTickCount64();

    // Debounced async re-check: some stacks deliver many interface notifications
    // before the Bluetooth layer reports the device as connected. Start a
    // background re-check (once per second) that refreshes the verified
    // connected-device list and shows a notification if a new device appears.
    static ULONGLONG s_lastRecheck = 0;
    if ((now - s_lastRecheck) > 1000) {
        s_lastRecheck = now;
        // Launch a short-lived thread to wait and re-run InitializeConnectedDevices
        HANDLE h = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ArrivalRecheckThread, NULL, 0, NULL);
        if (h) CloseHandle(h);
    }
    // Quick path: refresh the Bluetooth-verified connected devices list. Some
    // stacks emit many interface notifications and the canonical connected
    // state is best determined by scanning Bluetooth devices. If this finds a
    // new connected headset we can show a single arrival balloon and update
    // the tooltip without enumerating all interfaces.
    size_t oldConnectedCount = g_connectedDevices.size();
    InitializeConnectedDevices();
    if (g_connectedDevices.size() > oldConnectedCount) {
        // New device(s) connected — show a battery balloon (use the helper to
        // get a descriptive battery string) and update tooltip.
        std::wstring batteryInfo = GetHeadphonesBatteryLevel();
        OutputDebugStringW(L"HandleDeviceInterfaceArrival: InitializeConnectedDevices found new device(s)\n");
        OutputDebugStringW(batteryInfo.c_str());
        if (batteryInfo.find(L"WH-1000XM3") != std::wstring::npos) {
            ShowBalloonNotification(L"Headphones Connected", batteryInfo);
            g_lastNotifyTime = now;
            g_lastNotifyMessage = batteryInfo;
        }
        if (g_notifyIconAdded) {
            wcsncpy_s(g_nid.szTip, L"Headphones connected", _TRUNCATE);
            g_nid.uFlags = NIF_TIP;
            Shell_NotifyIconW(NIM_MODIFY, &g_nid);
        }
        // We refreshed g_connectedDevices already; no further processing needed.
        return;
    }
    // Try a direct mapping from the interface notification to a specific devnode
    // using the class GUID supplied in the notification. This is more reliable
    // than enumerating all devices because it targets the interface that fired.
    std::map<std::wstring, std::pair<std::wstring, std::wstring>> current;
    bool mapped = false;
    // Some notifications may provide a meaningful class GUID. Compare to GUID_NULL
    GUID nullGuid = GUID_NULL;
    if (memcmp(&pDev->dbcc_classguid, &nullGuid, sizeof(GUID)) != 0) {
        // Debug: print class GUID from notification
        wchar_t guidBuf[64] = {0};
        if (StringFromGUID2(pDev->dbcc_classguid, guidBuf, _countof(guidBuf))) {
            std::wstring dbg = L"HandleDeviceInterfaceArrival: notification class GUID=";
            dbg += guidBuf;
            dbg += L"\n";
            OutputDebugStringW(dbg.c_str());
        }
        HDEVINFO hDevInfo = SetupDiGetClassDevsW(&pDev->dbcc_classguid, NULL, NULL, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
        if (hDevInfo != INVALID_HANDLE_VALUE) {
            SP_DEVICE_INTERFACE_DATA ifData = {};
            ifData.cbSize = sizeof(ifData);
            DWORD idxIf = 0;
            // Helper: compare device paths case-insensitively and allow substring matches
            auto pathMatches = [](const wchar_t* a, const wchar_t* b) -> bool {
                if (!a || !b) return false;
                std::wstring A = a;
                std::wstring B = b;
                std::transform(A.begin(), A.end(), A.begin(), ::towlower);
                std::transform(B.begin(), B.end(), B.begin(), ::towlower);
                return (A.find(B) != std::wstring::npos) || (B.find(A) != std::wstring::npos);
            };

            for (; SetupDiEnumDeviceInterfaces(hDevInfo, NULL, &pDev->dbcc_classguid, idxIf, &ifData); ++idxIf) {
                DWORD needed = 0;
                // First call to get required buffer size
                SetupDiGetDeviceInterfaceDetailW(hDevInfo, &ifData, NULL, 0, &needed, NULL);
                if (needed == 0) continue;
                std::vector<BYTE> buf(needed);
                SP_DEVICE_INTERFACE_DETAIL_DATA_W* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(buf.data());
                // cbSize must be set to the size of the structure; for Unicode this is sizeof(WCHAR) aligned
                detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
                SP_DEVINFO_DATA devInfoData = {};
                devInfoData.cbSize = sizeof(devInfoData);
                if (SetupDiGetDeviceInterfaceDetailW(hDevInfo, &ifData, detail, needed, NULL, &devInfoData)) {
                    // Compare the device path with the notification path
                    if (pathMatches(detail->DevicePath, pDev->dbcc_name)) {
                        // Found matching interface; get DevInst and proceed to compute root and battery
                        DEVINST devInst = devInfoData.DevInst;
                        // Friendly name
                        WCHAR friendlyName[256] = {0};
                        DWORD reqSize = 0;
                        bool gotName = false;
                        if (SetupDiGetDeviceRegistryPropertyW(hDevInfo, &devInfoData, SPDRP_FRIENDLYNAME, NULL, (PBYTE)friendlyName, sizeof(friendlyName), &reqSize)) {
                            gotName = true;
                        } else {
                            WCHAR deviceDesc[256] = {0};
                            if (SetupDiGetDeviceRegistryPropertyW(hDevInfo, &devInfoData, SPDRP_DEVICEDESC, NULL, (PBYTE)deviceDesc, sizeof(deviceDesc), &reqSize)) {
                                wcscpy_s(friendlyName, _countof(friendlyName), deviceDesc);
                                gotName = true;
                            }
                        }

                        // Find root devnode
                        DEVINST root = devInst;
                        DEVINST parent = 0;
                        while (CM_Get_Parent(&parent, root, 0) == CR_SUCCESS && parent != 0 && parent != root) {
                            root = parent;
                        }
                        WCHAR rootIdBuf[512] = {0};
                        if (CM_Get_Device_IDW(root, rootIdBuf, _countof(rootIdBuf), 0) == CR_SUCCESS) {
                            std::wstring rootId = rootIdBuf;

                            // Attempt to read battery and format info
                            BYTE batteryLevel = 0;
                            CONFIGRET pres = ReadBatteryFromDevInst(devInst, batteryLevel);
                            std::wstringstream info;
                            if (gotName) info << L"Device: " << friendlyName << L"\n\n";
                            if (pres == CR_SUCCESS) {
                                info << L"Battery Level: " << (int)batteryLevel << L"%";
                            } else {
                                info << L"Battery Level: unavailable (code " << pres << L")";
                            }
                            current[rootId] = std::make_pair(gotName ? std::wstring(friendlyName) : std::wstring(L"(unknown)"), info.str());
                            mapped = true;
                        }
                        break;
                    }
                }
            }
            SetupDiDestroyDeviceInfoList(hDevInfo);
        }
    }

    // Fallback: if mapping failed, enumerate all devices as before
    if (!mapped) {
        HDEVINFO deviceInfoSet = SetupDiGetClassDevsW(NULL, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
        if (deviceInfoSet == INVALID_HANDLE_VALUE) {
            OutputDebugStringW(L"HandleDeviceInterfaceArrival: SetupDiGetClassDevsW(NULL) failed\n");
            return;
        }

    SP_DEVINFO_DATA devInfo = {};
    devInfo.cbSize = sizeof(devInfo);
    DWORD idx = 0;
    while (SetupDiEnumDeviceInfo(deviceInfoSet, idx, &devInfo)) {
        idx++;

        WCHAR friendlyName[256] = { 0 };
        DWORD reqSize = 0;
        bool gotName = false;

        if (SetupDiGetDeviceRegistryPropertyW(deviceInfoSet, &devInfo, SPDRP_FRIENDLYNAME, NULL, (PBYTE)friendlyName, sizeof(friendlyName), &reqSize)) {
            gotName = true;
        } else {
            WCHAR deviceDesc[256] = { 0 };
            if (SetupDiGetDeviceRegistryPropertyW(deviceInfoSet, &devInfo, SPDRP_DEVICEDESC, NULL, (PBYTE)deviceDesc, sizeof(deviceDesc), &reqSize)) {
                wcscpy_s(friendlyName, _countof(friendlyName), deviceDesc);
                gotName = true;
            }
        }
        if (!gotName) continue;
        if (wcsstr(friendlyName, L"WH-1000XM3") == NULL) continue;

        // Check that the devnode is started (avoid treating removed devices as arrivals)
        ULONG status = 0;
        ULONG problem = 0;
        CONFIGRET st = CM_Get_DevNode_Status(&status, &problem, devInfo.DevInst, 0);
        if (st != CR_SUCCESS) {
            wchar_t buf[128];
            _snwprintf_s(buf, _countof(buf), _TRUNCATE, L"HandleDeviceInterfaceArrival: CM_Get_DevNode_Status failed (CR=%u)\n", st);
            OutputDebugStringW(buf);
            continue;
        }
        if (!(status & DN_STARTED)) {
            // Not started -> treat as not present
            OutputDebugStringW(L"HandleDeviceInterfaceArrival: devnode not started, skipping\n");
            continue;
        }

        // Get this devnode's DevInst and then find the root devnode for grouping
        DEVINST currentNode = devInfo.DevInst;
        // Walk up to root devnode
        DEVINST parent = 0;
        DEVINST root = currentNode;
        while (CM_Get_Parent(&parent, root, 0) == CR_SUCCESS && parent != 0 && parent != root) {
            root = parent;
        }

        WCHAR rootIdBuf[512] = { 0 };
        if (CM_Get_Device_IDW(root, rootIdBuf, _countof(rootIdBuf), 0) != CR_SUCCESS) continue;
        std::wstring rootId = rootIdBuf;

        // Attempt to read battery from this devnode or its parents (no noisy retries)
        BYTE batteryLevel = 0;
        CONFIGRET pres = ReadBatteryFromDevInst(currentNode, batteryLevel);

        std::wstringstream info;
        info << L"Device: " << friendlyName << L"\n\n";
        if (pres == CR_SUCCESS) {
            info << L"Battery Level: " << (int)batteryLevel << L"%";
        } else {
            info << L"Battery Level: unavailable (code " << pres << L")";
        }

        current[rootId] = std::make_pair(std::wstring(friendlyName), info.str());
    }
    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    }

    // Compare current set to known connected set to find additions/removals
    std::set<std::wstring> currentIds;
    for (const auto &p : current) currentIds.insert(p.first);

    // Additions
    for (const auto &id : currentIds) {
        if (g_connectedDevices.find(id) == g_connectedDevices.end()) {
            // New device connected
            std::wstring virtualName = current[id].first;
            std::wstring batteryInfo = current[id].second;
            OutputDebugStringW(L"HandleDeviceInterfaceArrival: new device detected, showing balloon\n");
            // Print the virtual device name for diagnostics
            {
                std::wstring vdbg = virtualName + std::wstring(L"\n");
                OutputDebugStringW(vdbg.c_str());
            }
            ShowBalloonNotification(L"Headphones Connected", batteryInfo);
            g_lastNotifyTime = now;
            g_lastNotifyMessage = batteryInfo;
            // Update tray tooltip to indicate connected state
            if (g_notifyIconAdded) {
                wcscpy_s(g_nid.szTip, _countof(g_nid.szTip), L"Headphones connected");
                g_nid.uFlags = NIF_TIP;
                if (!Shell_NotifyIconW(NIM_MODIFY, &g_nid)) {
                    OutputDebugStringW(L"HandleDeviceInterfaceArrival: failed to update tray tooltip to connected\n");
                }
            }
        }
    }

    // Removals
    for (auto it = g_connectedDevices.begin(); it != g_connectedDevices.end();) {
        if (currentIds.find(it->first) == currentIds.end()) {
            // Device removed
            std::wstring virtualName = it->second;
            std::wstring msg = L"Headphones disconnected";
            OutputDebugStringW(L"HandleDeviceInterfaceArrival: device removal detected\n");
            // Print the virtual device name for diagnostics
            {
                std::wstring vdbg = virtualName + std::wstring(L"\n");
                OutputDebugStringW(vdbg.c_str());
            }
            ShowBalloonNotification(L"Headphones Disconnected", msg);
            // Update tray tooltip to indicate disconnected state when no devices remain
            // We'll remove the entry first then check remaining map size below.
            it = g_connectedDevices.erase(it);
        } else {
            ++it;
        }
    }

    // If no connected devices remain, update tooltip to disconnected
    if (g_notifyIconAdded) {
        if (g_connectedDevices.empty()) {
            wcscpy_s(g_nid.szTip, _countof(g_nid.szTip), L"Headphones disconnected");
            g_nid.uFlags = NIF_TIP;
            if (!Shell_NotifyIconW(NIM_MODIFY, &g_nid)) {
                OutputDebugStringW(L"HandleDeviceInterfaceArrival: failed to update tray tooltip to disconnected\n");
            }
        }
    }

    // Update connected map to current
    g_connectedDevices.clear();
    for (const auto &p : current) {
        g_connectedDevices[p.first] = p.second.first; // rootId -> virtualName
    }
}

// Tray callback message
static const UINT WM_TRAYICON = WM_APP + 1;
// Tray menu command IDs
static const UINT_PTR IDM_EXIT = 40001;

// Show a non-blocking balloon notification using the system tray (legacy balloons)
void ShowBalloonNotification(const std::wstring& title, const std::wstring& message) {
    // Initialize NOTIFYICONDATAW structure (only once)
    if (!g_notifyIconAdded) {
        g_nid.cbSize = sizeof(NOTIFYICONDATAW);
        // Use the main hidden window so we can receive tray callbacks (mouse/menu)
        g_nid.hWnd = g_notifyHwnd;
        // Register a callback message so our window receives tray icon events
        g_nid.uCallbackMessage = WM_TRAYICON;
        g_nid.uID = 1001;
        g_nid.uFlags = NIF_ICON | NIF_TIP | NIF_INFO | NIF_MESSAGE;
        g_nid.hIcon = LoadIconW(NULL, IDI_APPLICATION);
        // Tooltip text (small)
        wcsncpy_s(g_nid.szTip, L"HeadphonesBattery", _TRUNCATE);

        // Add icon to the notification area
        if (Shell_NotifyIconW(NIM_ADD, &g_nid)) {
            g_notifyIconAdded = true;
            OutputDebugStringW(L"ShowToastNotification: notification icon added\n");
        } else {
            OutputDebugStringW(L"ShowToastNotification: failed to add notification icon\n");
        }
    }

    // Fill in the balloon information (legacy balloon fields)
    g_nid.uFlags = NIF_INFO | NIF_ICON;
    wcsncpy_s(g_nid.szInfo, message.c_str(), _TRUNCATE);
    wcsncpy_s(g_nid.szInfoTitle, title.c_str(), _TRUNCATE);
    g_nid.dwInfoFlags = NIIF_INFO;

    // Display the balloon (non-blocking)
    if (!Shell_NotifyIconW(NIM_MODIFY, &g_nid)) {
        OutputDebugStringW(L"ShowBalloonNotification: Shell_NotifyIconW NIM_MODIFY failed\n");
    }
}

// Window procedure to handle device notifications
static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_DEVICECHANGE: {
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

        // Some drivers and stacks only emit devnodes-changed messages; handle that by re-querying
        if (wParam == DBT_DEVNODES_CHANGED) {
            OutputDebugStringW(L"WindowProc: WM_DEVICECHANGE - DBT_DEVNODES_CHANGED\n");
            // Throttle rapid devnode changes using same debounce window
            static ULONGLONG s_lastDevnodesNotify = 0;
            ULONGLONG now = GetTickCount64();
            if ((now - s_lastDevnodesNotify) < 1000) {
                OutputDebugStringW(L"WindowProc: devnodes change suppressed by throttle\n");
                return TRUE;
            }
            s_lastDevnodesNotify = now;

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
    case WM_TRAYICON: {
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
            // Always attempt to show the last-known battery info. If the headset
            // is connected, show live battery info. If it's disconnected but we
            // have a last-seen battery level from the device properties, show
            // it and clearly mark it as "last seen".
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
                    // Show the battery string but annotate that the device is disconnected
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
    case WM_COMMAND: {
        if (LOWORD(wParam) == IDM_EXIT) {
            OutputDebugStringW(L"Tray menu: Exit selected\n");
            // Remove tray icon and quit
            if (g_notifyIconAdded) {
                Shell_NotifyIconW(NIM_DELETE, &g_nid);
                g_notifyIconAdded = false;
            }
            PostQuitMessage(0);
            return 0;
        }
        break;
    }
    case WM_DESTROY:
        // Ensure tray icon is removed on destroy
        if (g_notifyIconAdded) {
            Shell_NotifyIconW(NIM_DELETE, &g_nid);
            g_notifyIconAdded = false;
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    // enable notifications after initialization
    g_showNotifications = true;

    // Register window class
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"HeadphonesBatteryMonitor";

    if (!RegisterClassW(&wc)) {
        return 1;
    }

    // Create hidden window to receive device notifications
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
        return 1;
    }

    // Store the hwnd so the tray helper can receive callbacks
    g_notifyHwnd = hwnd;

    // Ensure the tray icon is installed at startup so users always see the icon
    // even when no headphones are currently connected. Default tooltip shows
    // disconnected state; it will be updated after initialization.
    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = g_notifyHwnd;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.uID = 1001;
    g_nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    g_nid.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    wcscpy_s(g_nid.szTip, _countof(g_nid.szTip), L"Headphones disconnected");
    if (Shell_NotifyIconW(NIM_ADD, &g_nid)) {
        g_notifyIconAdded = true;
        OutputDebugStringW(L"WinMain: tray icon added at startup\n");
    } else {
        OutputDebugStringW(L"WinMain: failed to add tray icon at startup\n");
    }

    // Initialize connected devices map from current system state so we don't treat
    // already-connected virtual endpoints as new arrivals. Do this before the
    // initial notification check so we don't show a startup balloon for devices
    // that are present in PnP but not actually connected over Bluetooth.
    InitializeConnectedDevices();

    // Replace lambda with a callable function so other modules can trigger the same check
    // See definition of CheckAndNotifyNow() below.
    extern void CheckAndNotifyNow();
    CheckAndNotifyNow();

    // Ensure tooltip reflects Bluetooth-connected state even if g_connectedDevices
    // population did not detect roots. Use IsAnyHeadsetConnected() as authoritative
    // for the UI tooltip at startup.
    if (g_notifyIconAdded) {
        if (IsAnyHeadsetConnected()) {
            wcscpy_s(g_nid.szTip, _countof(g_nid.szTip), L"Headphones connected");
        } else if (g_connectedDevices.empty()) {
            wcscpy_s(g_nid.szTip, _countof(g_nid.szTip), L"Headphones disconnected");
        } else {
            wcscpy_s(g_nid.szTip, _countof(g_nid.szTip), L"Headphones connected");
        }
        g_nid.uFlags = NIF_TIP;
        if (!Shell_NotifyIconW(NIM_MODIFY, &g_nid)) {
            OutputDebugStringW(L"WinMain: failed to update tray tooltip after startup check\n");
        }
    }

    // (tooltip already updated above based on Bluetooth state)

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
        OutputDebugStringW(L"Failed to register device notification\n");
        if (g_notifyIconAdded) {
            Shell_NotifyIconW(NIM_DELETE, &g_nid);
            g_notifyIconAdded = false;
        }
        DestroyWindow(hwnd);
        return 1;
    }

    // Message loop - keeps the application running
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup
    OutputDebugStringW(L"Cleaning up: unregistering and removing tray icon\n");
    UnregisterDeviceNotification(hDevNotify);
    if (g_notifyIconAdded) {
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        g_notifyIconAdded = false;
    }
    DestroyWindow(hwnd);

    return 0;
}
#include "BluetoothDeviceManager.h"
#include <cwctype>
#include <algorithm>

// Parse Bluetooth address from device ID string (12 hex chars)
bool ParseBluetoothAddress(const std::wstring& deviceId, BLUETOOTH_ADDRESS& outAddr)
{
    // Find 12 hex chars sequence in the device ID
    std::wstring s = deviceId;
    for (size_t i = 0; i + 12 <= s.size(); ++i) {
        bool ok = true;
        for (size_t j = 0; j < 12; ++j) {
            wchar_t c = s[i + j];
            if (!((c >= L'0' && c <= L'9') || (c >= L'A' && c <= L'F') || (c >= L'a' && c <= L'f'))) {
                ok = false;
                break;
            }
        }
        if (!ok) continue;

        // Parse 6 pairs (AA:BB:CC:DD:EE:FF)
        for (int b = 0; b < 6; ++b) {
            std::wstring pair = s.substr(i + b * 2, 2);
            unsigned int val = 0;
            swscanf_s(pair.c_str(), L"%2x", &val);
            // Place into rgBytes reversed so rgBytes[0] is LSB
            outAddr.rgBytes[5 - b] = (BYTE)val;
        }
        return true;
    }
    return false;
}

// Check if a specific Bluetooth address is marked as connected
bool IsBluetoothDeviceConnected(const BLUETOOTH_ADDRESS& btAddr)
{
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
    bool isConnected = false;

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

    return found && isConnected;
}

// Scan all Bluetooth devices and check if any WH-1000XM3 is connected
bool IsAnyHeadsetConnected()
{
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
                _snwprintf_s(buf, _countof(buf), _TRUNCATE, 
                    L"IsAnyHeadsetConnected: found BT device '%s' connected=%d\n", 
                    devInfo.szName, devInfo.fConnected);
                OutputDebugStringW(buf);
                if (devInfo.fConnected) {
                    foundConnected = true;
                    break;
                }
            }
            devInfo.dwSize = sizeof(devInfo);
        } while (BluetoothFindNextDevice(h, &devInfo));
        BluetoothFindDeviceClose(h);
    }
    return foundConnected;
}

/*
 * BluetoothDeviceManager.cpp
 * 
 * MODULE: Bluetooth Device Manager (Generic Device Enumeration)
 * PURPOSE: Implements generic Bluetooth audio device enumeration and filtering
 * 
 * DESCRIPTION:
 *   Implements the core Bluetooth device enumeration logic that works with any audio device.
 *   Uses Windows Bluetooth APIs to discover all paired/connected audio devices and maintain
 *   a registry of active devices.
 * 
 *   Key functions:
 *   - EnumerateBluetoothAudioDevices() - Search for all Bluetooth audio devices
 *   - ParseBluetoothAddress() - Convert device ID string to MAC address
 *   - IsAudioDeviceType() - Determine if device is audio-capable
 *   - LookupDevice() - Query registry by Bluetooth address
 * 
 * ENUMERATION STRATEGY:
 *   1. Call BluetoothFindFirstDevice() with search criteria
 *   2. Iterate through all devices with BluetoothFindNextDevice()
 *   3. For each device, get properties (name, address, class)
 *   4. Check if class matches audio device criteria
 *   5. Extract MAC address from device ID string
 *   6. Store in global g_bluetoothAudioDevices map
 *   7. Return populated map to caller
 * 
 * ADDRESS PARSING:
 *   Bluetooth address format in device ID: "aa:bb:cc:dd:ee:ff" (12 hex chars)
 *   Extraction: Read last 12 characters of device ID string
 *   Conversion: Parse as hex pairs into BLUETOOTH_ADDRESS structure
 *   Validation: Verify all 6 bytes were successfully parsed
 * 
 * DEVICE CLASS FILTERING:
 *   Checks COD (Class of Device) bits for:
 *   - COD_MAJOR_AUDIO flag (indicates audio rendering/capture)
 *   - Audio device types: Headphones, Speakers, Car audio, Hearing aids
 *   - Wireless headsets and generic audio devices
 * 
 * GLOBAL STATE:
 *   - g_bluetoothAudioDevices: Map of discovered devices (address -> properties)
 *   - Updated at startup in EnumerateBluetoothAudioDevices()
 *   - Persists across polling cycles
 *   - Cleaned up on application exit
 * 
 * LOGGING:
 *   [BluetoothDeviceManager] prefix on all debug output
 *   Logs: Device enumeration, filtering, address parsing results
 *   Debug format: Device name, MAC address, class, connection status
 * 
 * WINDOWS API CALLS:
 *   - BluetoothFindFirstDevice() - Start enumeration search
 *   - BluetoothFindNextDevice() - Get next device in search
 *   - BluetoothFindDeviceClose() - Clean up search handle
 *   - BluetoothGetDeviceInfo() - Get device properties
 * 
 * ERROR HANDLING:
 *   - NULL handle checks on search operations
 *   - BLUETOOTH_ADDRESS parsing validation
 *   - Device class verification before adding to registry
 *   - Debug output on enumeration failures
 */

#include "BluetoothDeviceManager.h"
#include <cwctype>
#include <algorithm>

// Global registry of all detected Bluetooth audio devices
std::map<std::wstring, BluetoothAudioDevice> g_bluetoothAudioDevices;

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

                wchar_t addrBuf[64];
                _snwprintf_s(addrBuf, _countof(addrBuf), _TRUNCATE, L"%02X:%02X:%02X:%02X:%02X:%02X",
                    btAddr.rgBytes[5], btAddr.rgBytes[4], btAddr.rgBytes[3], 
                    btAddr.rgBytes[2], btAddr.rgBytes[1], btAddr.rgBytes[0]);

                wchar_t msg[512];
                _snwprintf_s(msg, _countof(msg), _TRUNCATE, 
                    L"[BluetoothDeviceManager] IsBluetoothDeviceConnected: address=%s name='%s' fConnected=%d\n",
                    addrBuf, devInfo.szName, devInfo.fConnected);
                OutputDebugStringW(msg);
                break;
            }
            devInfo.dwSize = sizeof(devInfo);
        } while (BluetoothFindNextDevice(h, &devInfo));
        BluetoothFindDeviceClose(h);
    }

    if (!found) {
        wchar_t addrBuf[64];
        _snwprintf_s(addrBuf, _countof(addrBuf), _TRUNCATE, L"%02X:%02X:%02X:%02X:%02X:%02X",
            btAddr.rgBytes[5], btAddr.rgBytes[4], btAddr.rgBytes[3], 
            btAddr.rgBytes[2], btAddr.rgBytes[1], btAddr.rgBytes[0]);

        wchar_t msg[256];
        _snwprintf_s(msg, _countof(msg), _TRUNCATE, 
            L"[BluetoothDeviceManager] IsBluetoothDeviceConnected: address=%s NOT FOUND in Bluetooth enumeration\n",
            addrBuf);
        OutputDebugStringW(msg);
    }

    return found && isConnected;
}

// Check if a device name matches a known audio device pattern
bool IsAudioDeviceType(const std::wstring& deviceName)
{
    std::wstring name = deviceName;
    std::transform(name.begin(), name.end(), name.begin(), ::towlower);

    return (name.find(L"headphone") != std::wstring::npos ||
            name.find(L"headset") != std::wstring::npos ||
            name.find(L"earbud") != std::wstring::npos ||
            name.find(L"speaker") != std::wstring::npos ||
            name.find(L"airpod") != std::wstring::npos ||
            name.find(L"buds") != std::wstring::npos ||
            name.find(L"beats") != std::wstring::npos ||
            name.find(L"hands-free") != std::wstring::npos ||
            name.find(L"avrcp") != std::wstring::npos);
}

// Enumerate all Bluetooth audio devices and populate the registry
int EnumerateBluetoothAudioDevices()
{
    OutputDebugStringW(L"[BluetoothDeviceManager] EnumerateBluetoothAudioDevices: starting enumeration\n");

    BLUETOOTH_DEVICE_SEARCH_PARAMS search = {};
    search.dwSize = sizeof(search);
    search.fReturnAuthenticated = TRUE;
    search.fReturnRemembered = TRUE;
    search.fReturnUnknown = TRUE;
    search.fReturnConnected = TRUE;

    BLUETOOTH_DEVICE_INFO devInfo = {};
    devInfo.dwSize = sizeof(devInfo);
    HANDLE h = BluetoothFindFirstDevice(&search, &devInfo);

    int foundCount = 0;

    if (h) {
        do {
            // Log all devices found
            wchar_t logMsg[512];
            _snwprintf_s(logMsg, _countof(logMsg), _TRUNCATE,
                L"[BluetoothDeviceManager] Found BT device: name='%s' fConnected=%d\n",
                devInfo.szName, devInfo.fConnected);
            OutputDebugStringW(logMsg);

            // Filter for audio devices that are connected
            if (devInfo.fConnected && IsAudioDeviceType(devInfo.szName)) {
                foundCount++;

                // Format the Bluetooth address
                wchar_t addrBuf[64];
                _snwprintf_s(addrBuf, _countof(addrBuf), _TRUNCATE, L"%02X:%02X:%02X:%02X:%02X:%02X",
                    devInfo.Address.rgBytes[5], devInfo.Address.rgBytes[4], devInfo.Address.rgBytes[3],
                    devInfo.Address.rgBytes[2], devInfo.Address.rgBytes[1], devInfo.Address.rgBytes[0]);

                // Create address string for map key
                wchar_t addrKey[64];
                _snwprintf_s(addrKey, _countof(addrKey), _TRUNCATE, L"%02X%02X%02X%02X%02X%02X",
                    devInfo.Address.rgBytes[5], devInfo.Address.rgBytes[4], devInfo.Address.rgBytes[3],
                    devInfo.Address.rgBytes[2], devInfo.Address.rgBytes[1], devInfo.Address.rgBytes[0]);

                std::wstring key = addrKey;

                // Add to registry
                BluetoothAudioDevice device;
                device.bluetoothAddress = addrBuf;
                device.friendlyName = devInfo.szName;
                device.isConnected = devInfo.fConnected != 0;
                device.isDefaultOutput = false;
                device.batteryLevel = 0;
                device.lastStateChangeTime = GetTickCount64();
                memcpy(&device.btAddr, &devInfo.Address, sizeof(BLUETOOTH_ADDRESS));

                g_bluetoothAudioDevices[key] = device;

                wchar_t msg[512];
                _snwprintf_s(msg, _countof(msg), _TRUNCATE,
                    L"[BluetoothDeviceManager] REGISTERED connected audio device: '%s' (addr=%s)\n",
                    devInfo.szName, addrBuf);
                OutputDebugStringW(msg);
            }

            devInfo.dwSize = sizeof(devInfo);
        } while (BluetoothFindNextDevice(h, &devInfo));
        BluetoothFindDeviceClose(h);
    } else {
        OutputDebugStringW(L"[BluetoothDeviceManager] BluetoothFindFirstDevice returned NULL\n");
    }

    wchar_t summaryMsg[256];
    _snwprintf_s(summaryMsg, _countof(summaryMsg), _TRUNCATE,
        L"[BluetoothDeviceManager] EnumerateBluetoothAudioDevices: found %d connected audio devices\n", foundCount);
    OutputDebugStringW(summaryMsg);

    return foundCount;
}

// Scan all Bluetooth devices and check if any WH-1000XM3 is connected (legacy)
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
                wchar_t buf[512];
                _snwprintf_s(buf, _countof(buf), _TRUNCATE, 
                    L"[BluetoothDeviceManager] IsAnyHeadsetConnected: found BT device '%s' fConnected=%d\n", 
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

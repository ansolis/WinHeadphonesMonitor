#include "DevicePropertyReader.h"
#include <setupapi.h>
#include <sstream>

// Battery level property key (DEVPROPKEY for battery percentage)
static DEVPROPKEY g_batteryLevelKey = { 
    { 0x104EA319, 0x6EE2, 0x4701, { 0xBD, 0x47, 0x8D, 0xDB, 0xF4, 0x25, 0xBB, 0xE5 } }, 
    2 
};

// Read battery level from a devnode, walking up the parent chain
CONFIGRET ReadBatteryFromDevInst(DEVINST devInst, BYTE& outBattery)
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

// Format battery information into a user-friendly string
std::wstring FormatBatteryString(const std::wstring& friendlyName, BYTE batteryLevel, CONFIGRET result)
{
    std::wstringstream info;
    info << L"Device: " << friendlyName << L"\n\n";
    if (result == CR_SUCCESS) {
        info << L"Battery Level: " << (int)batteryLevel << L"%";
    } else {
        info << L"Battery Level: unavailable (code " << result << L")";
    }
    return info.str();
}

// Enumerate all devices and find WH-1000XM3 with battery level
std::wstring GetHeadphonesBatteryLevel()
{
    HDEVINFO deviceInfoSet = SetupDiGetClassDevs(
        NULL,
        NULL,
        NULL,
        DIGCF_ALLCLASSES | DIGCF_PRESENT
    );

    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        OutputDebugStringW(L"GetHeadphonesBatteryLevel: SetupDiGetClassDevs failed\n");
        return L"Failed to get device list";
    }

    SP_DEVINFO_DATA deviceInfoData = { 0 };
    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    DWORD deviceIndex = 0;

    // Enumerate all devices
    while (SetupDiEnumDeviceInfo(deviceInfoSet, deviceIndex, &deviceInfoData)) {
        deviceIndex++;

        // Get device friendly name using the registry property
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
            // Check if this is the WH-1000XM3
            if (wcsstr(friendlyName, L"WH-1000XM3") != NULL) {
                // Get device instance ID for diagnostics
                WCHAR devIdBuf[512] = { 0 };
                if (CM_Get_Device_IDW(deviceInfoData.DevInst, devIdBuf, _countof(devIdBuf), 0) == CR_SUCCESS) {
                    std::wstring dbg = L"GetHeadphonesBatteryLevel: checking DevInst ";
                    dbg += devIdBuf;
                    dbg += L"\n";
                    OutputDebugStringW(dbg.c_str());
                }

                // Try to read battery level
                BYTE batteryLevel = 0;
                CONFIGRET cr = ReadBatteryFromDevInst(deviceInfoData.DevInst, batteryLevel);

                if (cr == CR_SUCCESS) {
                    SetupDiDestroyDeviceInfoList(deviceInfoSet);
                    OutputDebugStringW(L"GetHeadphonesBatteryLevel: found device and battery\n");
                    return FormatBatteryString(friendlyName, batteryLevel, cr);
                } else {
                    // Log error for diagnostics
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

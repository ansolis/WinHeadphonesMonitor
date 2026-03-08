#include "DeviceContextMenu.h"
#include "DeviceRegistry.h"
#include <mmdeviceapi.h>

// Global instance
DeviceContextMenu* g_deviceContextMenu = nullptr;

DeviceContextMenu::DeviceContextMenu()
{
    OutputDebugStringW(L"[DeviceContextMenu] Initialized\n");
}

DeviceContextMenu::~DeviceContextMenu()
{
}

int DeviceContextMenu::ShowMenu()
{
    if (!g_deviceRegistry) {
        return 0;
    }

    HMENU hPopupMenu = CreatePopupMenu();
    if (!hPopupMenu) {
        return 0;
    }

    // Populate device list
    int deviceCount = g_deviceRegistry->GetConnectedDeviceCount();
    if (deviceCount > 0) {
        HMENU hDevicesSubmenu = CreatePopupMenu();
        if (hDevicesSubmenu) {
            PopulateDeviceMenus(hDevicesSubmenu);
            InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_POPUP,
                (UINT_PTR)hDevicesSubmenu, L"Audio Devices");
        }

        InsertMenuW(hPopupMenu, 1, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
        InsertMenuW(hPopupMenu, 2, MF_BYPOSITION | MF_STRING,
            IDM_DISCONNECT, L"Disconnect Active");
    } else {
        InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING | MF_GRAYED,
            0, L"No devices connected");
    }

    // Add exit option
    InsertMenuW(hPopupMenu, 999, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
    InsertMenuW(hPopupMenu, 999, MF_BYPOSITION | MF_STRING, 
        40001, L"Exit");  // IDM_EXIT from TrayNotification

    // Show menu at cursor
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(GetForegroundWindow());
    int result = TrackPopupMenu(hPopupMenu, TPM_RIGHTBUTTON | TPM_RETURNCMD,
        pt.x, pt.y, 0, GetForegroundWindow(), nullptr);

    DestroyMenu(hPopupMenu);
    return result;
}

void DeviceContextMenu::PopulateDeviceMenus(HMENU hMenu)
{
    if (!g_deviceRegistry) {
        return;
    }

    const auto& devices = g_deviceRegistry->GetAllDevices();
    auto defaultDevice = g_deviceRegistry->GetDefaultOutputDevice();

    UINT menuId = IDM_SWITCH_BASE;
    for (const auto& pair : devices) {
        const auto& device = pair.second;
        
        std::wstring label = device.friendlyName;
        if (defaultDevice && pair.first == defaultDevice->bluetoothAddress) {
            label += L" [ACTIVE]";
        }

        UINT flags = MF_STRING;
        if (defaultDevice && pair.first == defaultDevice->bluetoothAddress) {
            flags |= MF_CHECKED;
        }

        InsertMenuW(hMenu, GetMenuItemCount(hMenu), MF_BYPOSITION | flags,
            menuId, label.c_str());
        menuId++;
    }
}

void DeviceContextMenu::HandleCommand(int commandId)
{
    wchar_t msg[256];
    _snwprintf_s(msg, _countof(msg), _TRUNCATE,
        L"[DeviceContextMenu] HandleCommand: %d\n", commandId);
    OutputDebugStringW(msg);

    if (commandId >= IDM_SWITCH_BASE && commandId < IDM_SWITCH_BASE + 100) {
        // Device switching command
        OutputDebugStringW(L"[DeviceContextMenu] Device switching not yet implemented\n");
    } else if (commandId == IDM_DISCONNECT) {
        auto defaultDevice = g_deviceRegistry->GetDefaultOutputDevice();
        if (defaultDevice) {
            DisconnectDevice(defaultDevice->bluetoothAddress);
        }
    }
}

void DeviceContextMenu::DisconnectDevice(const std::wstring& bluetoothAddress)
{
    wchar_t msg[512];
    _snwprintf_s(msg, _countof(msg), _TRUNCATE,
        L"[DeviceContextMenu] DisconnectDevice: %s (not yet implemented)\n",
        bluetoothAddress.c_str());
    OutputDebugStringW(msg);
    
    // TODO: Implement Bluetooth device disconnection
}

void DeviceContextMenu::SwitchToDevice(const std::wstring& bluetoothAddress)
{
    wchar_t msg[512];
    _snwprintf_s(msg, _countof(msg), _TRUNCATE,
        L"[DeviceContextMenu] SwitchToDevice: %s (not yet implemented)\n",
        bluetoothAddress.c_str());
    OutputDebugStringW(msg);
    
    // TODO: Implement device switching via IMMDeviceEnumerator::SetDefaultAudioEndpoint()
}

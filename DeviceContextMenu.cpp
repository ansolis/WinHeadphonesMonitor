/*
 * DeviceContextMenu.cpp
 * 
 * MODULE: Device Context Menu (Tray Icon Controls)
 * PURPOSE: Implements right-click context menu functionality for device control
 * 
 * DESCRIPTION:
 *   Provides implementation for right-click context menu shown on tray icon.
 *   Queries DeviceRegistry to build dynamic menu items for all connected devices.
 *   Handles menu display, user selection, and command processing.
 * 
 * KEY OPERATIONS:
 *   - ShowMenu() - Create and display menu at cursor position
 *   - PopulateDeviceMenus() - Build menu items from DeviceRegistry
 *   - HandleCommand() - Process menu selection (ID-based routing)
 *   - UpdateMenuItems() - Refresh menu when device state changes
 * 
 * MENU CONSTRUCTION:
 *   1. Get current device list from DeviceRegistry
 *   2. Assign sequential menu IDs to each device
 *   3. Add device names with status indicators
 *   4. Add separator
 *   5. Add Exit option
 *   6. Display menu with TrackPopupMenu()
 * 
 * STATUS INDICATORS:
 *   - "(ACTIVE OUTPUT)" - Device is primary audio output (Stage 3)
 *   - "(connected)" - Device connected but not default (Stage 2)
 *   - "(disconnected)" - Device no longer available
 *   - No indicator - Unknown state
 * 
 * COMMAND HANDLING:
 *   Currently handles:
 *   - IDM_EXIT: Application exit command
 *   
 *   Future expansion for:
 *   - Device selection (set as default output)
 *   - Device disconnect
 *   - Properties dialog
 *   - Battery level display
 * 
 * LOGGING:
 *   [DeviceContextMenu] prefix on all debug output
 *   Examples:
 *     [DeviceContextMenu] Initialized
 *     [DeviceContextMenu] ShowMenu invoked at 640, 480
 *     [DeviceContextMenu] PopulateDeviceMenus: Found 2 devices
 *     [DeviceContextMenu] HandleCommand: ID=40002 (device select)
 * 
 * INTEGRATION:
 *   - Called by: WindowManager on tray icon right-click
 *   - Uses: DeviceRegistry for device list and status
 *   - Reads: Device names, connection state, default output flag
 *   - Outputs: Menu display via OS and command results
 * 
 * IMPLEMENTATION NOTES:
 *   - TrackPopupMenu() blocks until user selection
 *   - Menu IDs: Custom range starting from IDM_DEVICE_BASE
 *   - Global instance: g_deviceContextMenu (created in WinMain)
 *   - Device count determined dynamically at menu display time
 */

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

#pragma once

#include <windows.h>
#include <string>
#include <vector>

// Context menu manager for tray icon device control
class DeviceContextMenu {
public:
    DeviceContextMenu();
    ~DeviceContextMenu();

    // Show context menu at cursor position
    // Returns command ID or 0 if cancelled
    int ShowMenu();

    // Handle menu command
    void HandleCommand(int commandId);

private:
    static const UINT IDM_DEVICES_BASE = 5000;
    static const UINT IDM_DISCONNECT = 5100;
    static const UINT IDM_SWITCH_BASE = 6000;

    // Populate menu items from device registry
    void PopulateDeviceMenus(HMENU hMenu);

    // Disconnect the specified device
    void DisconnectDevice(const std::wstring& bluetoothAddress);

    // Switch to device (set as default output)
    void SwitchToDevice(const std::wstring& bluetoothAddress);
};

// Global instance
extern DeviceContextMenu* g_deviceContextMenu;

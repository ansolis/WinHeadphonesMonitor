/*
 * DeviceContextMenu.h
 * 
 * MODULE: Device Context Menu (Tray Icon Controls)
 * PURPOSE: Right-click context menu for device control and status display
 * 
 * DESCRIPTION:
 *   Implements context menu that appears when user right-clicks the tray icon.
 *   Displays all connected Bluetooth audio devices with their current status
 *   (connected/disconnected, default output indicator).
 * 
 *   Currently provides:
 *   - Device list with status indicators
 *   - Connection status display
 *   - Default output device highlight
 *   - Exit menu option
 * 
 *   Future enhancements can add:
 *   - Device switching (set as default output)
 *   - Disconnect device action
 *   - Device properties dialog
 *   - Battery level display in menu
 * 
 * USAGE:
 *   1. Global instance: extern DeviceContextMenu* g_deviceContextMenu (created in main)
 *   2. Initialize: g_deviceContextMenu = new DeviceContextMenu()
 *   3. Show menu: g_deviceContextMenu->ShowMenu(x, y) (called from tray click)
 *   4. Menu automatically queries DeviceRegistry for current device state
 *   5. User can select device or exit (exit handled by main loop)
 * 
 * KEY METHODS:
 *   - ShowMenu(x, y) - Display context menu at specified screen coordinates
 *   - PopulateDeviceMenus() - Build menu items from current device registry
 *   - HandleCommand(id) - Process menu selection (currently: EXIT only)
 *   - UpdateMenuItems() - Refresh menu items when devices change
 * 
 * FEATURES:
 *   - Automatic device listing from DeviceRegistry
 *   - Status indicators for connected/disconnected devices
 *   - Default output device marked with indicator
 *   - Clean separation from tray notification logic
 * 
 * INTEGRATION:
 *   - Called by: WindowManager on WM_TRAYICON right-click
 *   - Uses: DeviceRegistry for device information
 *   - Manages: Right-click menu display and command handling
 *   - Outputs: Debug logging of menu operations
 * 
 * MENU STRUCTURE:
 *   Example menu:
 *     [Device A] (ACTIVE OUTPUT)
 *     [Device B] (connected)
 *     ---
 *     Exit
 * 
 * LOGGING:
 *   [DeviceContextMenu] prefix on all debug output
 *   Examples:
 *     [DeviceContextMenu] ShowMenu: 640x480
 *     [DeviceContextMenu] PopulateDeviceMenus: 2 devices, 1 default
 *     [DeviceContextMenu] HandleCommand: EXIT
 */

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

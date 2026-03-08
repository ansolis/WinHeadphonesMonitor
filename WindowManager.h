/*
 * WindowManager.h
 * 
 * MODULE: Window Manager (Message Handling & Device Notifications)
 * PURPOSE: Create message-only window to receive Windows device and tray notifications
 * 
 * DESCRIPTION:
 *   Creates a hidden message-only window that serves as the central message dispatcher
 *   for the application. Receives device change notifications (WM_DEVICECHANGE),
 *   tray icon notifications (WM_TRAYICON), and routes them to appropriate handlers.
 * 
 *   Message-only windows:
 *   - Invisible (not displayed on screen)
 *   - Cannot be enumerated (invisible to Alt+Tab)
 *   - Receive all Windows messages like normal windows
 *   - Perfect for background applications and system monitoring
 * 
 * USAGE:
 *   1. Register window class: RegisterWindowClass(hInstance)
 *   2. Create hidden window: CreateDeviceMonitorWindow(hInstance)
 *   3. Message loop: GetMessage/DispatchMessage in WinMain
 *   4. Cleanup: Destroy window and deregister class on exit
 * 
 * KEY FUNCTIONS:
 *   - RegisterWindowClass() - Register window class for monitoring
 *   - CreateDeviceMonitorWindow() - Create hidden message-only window
 *   - WindowProc() - Callback handling device change and tray messages
 * 
 * WINDOWS MESSAGES HANDLED:
 *   - WM_DEVICECHANGE: Bluetooth device arrival/removal notification
 *     - DBT_DEVICEARRIVAL: New device detected (Stage 1)
 *     - DBT_DEVICEREMOVEPENDING: Device being removed
 *     - DBT_DEVICEREMOVECOMPLETE: Device fully removed
 *     - Triggers HandleDeviceInterfaceArrival() on arrival
 *   
 *   - WM_TRAYICON: Tray icon callback (left/right click)
 *     - Routes to DeviceContextMenu->ShowMenu() on right-click
 *     - Handles icon tooltip display
 *   
 *   - Other: Standard Windows messages (WM_CREATE, WM_DESTROY, etc.)
 * 
 * DEVICE NOTIFICATION REGISTRATION:
 *   Uses RegisterDeviceNotification() to subscribe to device events:
 *   - DEVICE_NOTIFY_WINDOW_HANDLE: Register for this window
 *   - DBT_DEVTYP_DEVICEINTERFACE: Interested in interface arrivals
 *   - GUID_DEVINTERFACE_BLUETOOTH_RADIO: Subscribe to Bluetooth events
 * 
 * MESSAGE ROUTING:
 *   WM_DEVICECHANGE -> HandleDeviceInterfaceArrival()
 *                   -> Stage 1 detection
 *                   -> Add to DeviceRegistry
 *                   -> Start Stage2Verifier polling
 *                   -> Show Stage 1 notification
 * 
 *   WM_TRAYICON (right-click) -> DeviceContextMenu->ShowMenu()
 *                              -> Display device list
 *                              -> Handle user selection
 * 
 * WINDOW CLASS:
 *   - Class name: unique HWND identifier for message-only window
 *   - Style: CS_VREDRAW | CS_HREDRAW (standard)
 *   - Procedure: WindowProc for message handling
 *   - Instance: Current application HINSTANCE
 * 
 * MESSAGE-ONLY WINDOW:
 *   Created with:
 *   - Parent: HWND_MESSAGE (special parent for message-only windows)
 *   - Coordinates: Ignored (message-only, invisible)
 *   - Result: Window that receives messages but isn't visible
 * 
 * INTEGRATION:
 *   - Called by: WinMain for initialization
 *   - Uses: Device notification APIs to subscribe to Bluetooth events
 *   - Calls: HandleDeviceInterfaceArrival() on device detection
 *   - Calls: DeviceContextMenu->ShowMenu() on right-click
 *   - Outputs: Debug logging of all messages
 * 
 * LOGGING:
 *   [WindowManager] prefix on all debug output
 *   Examples:
 *     [WindowManager] RegisterWindowClass: Success
 *     [WindowManager] CreateDeviceMonitorWindow: HWND=0x00020456
 *     [WindowManager] WM_DEVICECHANGE: DBT_DEVICEARRIVAL
 *     [WindowManager] WM_TRAYICON: Right-click at 640,480
 * 
 * THREAD SAFETY:
 *   - Window messages processed on main thread
 *   - All handlers run on main thread
 *   - Device notification thread-safe (OS-managed)
 *   - No background threads needed for message dispatch
 */

#pragma once

#include <windows.h>

// Register the window class for device monitoring
// Returns true if successful
bool RegisterWindowClass(HINSTANCE hInstance);

// Create a hidden message-only window to receive device notifications
// Returns window handle or NULL on failure
HWND CreateDeviceMonitorWindow(HINSTANCE hInstance);

// Window procedure for handling device notifications and tray events
// Registered for the hidden window
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

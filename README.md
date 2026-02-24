# Headphones Battery Level Display

A minimal C++ Win32 application that displays the battery level of your Bluetooth headphones.
Monitors your Sony WH-1000XM3 Bluetooth headphones for connection/disconnection and provides battery status.
Installs a system tray icon with information about the headphones.

## Features
- Retrieves battery level using Windows Device Property APIs
- Displays battery level in a simple message box
- Specifically configured for WH-1000XM3 (but can be modified for other devices)

## Requirements
- Visual Studio 2026
- Windows 10 or later

## How to Build

1. Open `HeadphonesBattery.sln` in [Visual Studio](https://visualstudio.microsoft.com/downloads/)
2. Select your build configuration (Debug or Release)
3. Select your platform (x64 recommended)
4. Press F5 to build and run, or Ctrl+Shift+B to just build

## How It Works

The application:
1. **Shows initial battery level** when you launch it
2. **Stays running in the background** (as a hidden window)
3. **Monitors for device connections** and shows a notification when your headphones connect/power on
4. Notifications only appear AFTER you dismiss the initial dialog

The application uses:
- **SetupAPI**: To enumerate devices
- **Configuration Manager API**: To retrieve battery levels without linker issues
- **Device Notifications**: RegisterDeviceNotification to detect when devices connect
- **Hidden Window**: A message-only window that receives WM_DEVICECHANGE messages
- **Message Loop**: Keeps the application running to receive device events

## Exiting the Application

Since the application runs in the background, you'll need to:
- Open Task Manager (Ctrl+Shift+Esc)
- Find "HeadphonesBattery.exe" under Processes
- Right-click and select "End Task"

Alternatively, you can add a system tray icon for easy exit (see customization below).

## Customization

To monitor a different device, modify the device name check in `HeadphonesBattery.cpp`:

```cpp
if (wcsstr(friendlyName, L"WH-1000XM3") != NULL) {
    // Change "WH-1000XM3" to your device name
}
```

**Advanced customizations:**
- **Add system tray icon**: Use Shell_NotifyIcon() to add an icon that allows right-click exit
- **Add hotkey to exit**: Register a global hotkey with RegisterHotKey()
- **Log battery levels**: Write to a file instead of/in addition to showing message boxes
- **Monitor multiple devices**: Remove the device name check or check for multiple names
- **Change notification trigger**: Currently triggers on any device change; you could make it more specific
- **Display as toast notification**: Use Windows 10+ toast notifications instead of MessageBox

## Files Included

- `HeadphonesBattery.cpp` - Main source code
- `HeadphonesBattery.vcxproj` - Visual Studio project file
- `HeadphonesBattery.sln` - Visual Studio solution file
- `README.md` - This file

## Notes

- The device must be connected and actively reporting battery level
- Not all Bluetooth devices report battery level to Windows
- The application requires Windows 10 SDK headers (setupapi.h, devpkey.h, cfgmgr32.h)
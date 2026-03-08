# HeadphonesBattery Monitor

**A production-ready Windows Bluetooth audio device monitor that tracks any connected audio device and notifies you of connection, disconnection, and primary output changes.**

## Features ✨

### ✅ Multi-Device Support
- Monitors **any** Bluetooth audio device (not just one specific model)
- Supports simultaneous connections to multiple devices
- Works with: Sony WH-1000XM3, AirPods, Beats, Samsung Buds, and more

### ✅ Three-Stage Notifications
- **Stage 1:** "Detecting [device name]..." - Early driver detection
- **Stage 2:** "[Device name] connected" - Audio endpoint ready
- **Stage 3:** "Now using [device] for audio" - Became primary output
- **Disconnected:** "[Device name] disconnected" - Device removed

### ✅ Smart Tray Integration
- Device status in tray tooltip
- Single device: `"WH-1000XM3 (ACTIVE OUTPUT)"` or `"WH-1000XM3 (standby)"`
- Multiple devices: `"WH-1000XM3 (ACTIVE) + 1 other"`
- Right-click context menu for device control
- Left-click to view current battery status

### ✅ Windows Audio API Integration
- Real-time MMDevice notifications
- Detects when device becomes primary output
- No polling required for audio endpoint detection

### ✅ Comprehensive Logging
- All device events logged with timestamps
- Device names and Bluetooth addresses in logs
- Stage markers for debugging
- Module-prefixed output for easy filtering

## Installation

### Requirements
- Windows 10 or later
- Visual Studio 2019+ (for building from source)
- Bluetooth adapter with audio device support

### Build from Source

1. Clone the repository:
```bash
git clone https://github.com/ansolis/WinHeadphonesMonitor.git
cd WinHeadphonesMonitor
```

2. Open `HeadphonesBattery.vcxproj` in Visual Studio 2019+

3. Build the solution (Build → Build Solution)

4. Run the executable from `x64/Debug/` or `x64/Release/`

## Usage

### Basic Operation

1. **Connect a Bluetooth audio device** to your PC
2. **Application starts automatically** at startup (place shortcut in Startup folder)
3. **Monitor in system tray** - see device status in tooltip
4. **Receive notifications** for each stage of device lifecycle

### Tray Icon Interaction

**Left-click:** View current battery status and connection state
**Right-click:** Device menu with options (Disconnect, Switch, Exit)
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
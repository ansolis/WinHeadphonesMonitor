# Project Overview

This repository contains a small Win32 tray application that monitors the battery level and connection state of Sony `WH-1000XM3` (and similar) Bluetooth headphones. The app runs as a background message-only window with a notification area (system tray) icon and shows legacy balloon notifications and an on-hover tooltip.

Application features

- Install a system tray icon at startup so users always have a visible entry.
- Detect `WH-1000XM3` headphone devices via device enumeration (SetupAPI / cfgmgr32) and Bluetooth APIs.
- Read a shared battery property using `CM_Get_DevNode_PropertyW` (walks up devnode parents if necessary) and display battery percentage.
- Show a balloon notification on headphone connect and disconnect.
- Update the tray tooltip to reflect `Headphones connected` / `Headphones disconnected` state.
- Provide a right-click context menu on the tray icon with an Exit command.
- Left-click the tray icon to show a modal box with current battery level or an error message.
- Debounce rapid notifications and suppress duplicate balloons (time-window based).
- Attempt to map incoming device-interface notifications to a specific devnode/interface and only show arrival for verified connections.
- Use Bluetooth APIs to verify true Bluetooth connection state (fConnected) before showing startup notification.

Logic flow / high-level behavior

1. On startup:
   - Create a message-only hidden window to receive device notifications.
   - Add a notification icon to the tray immediately (default tooltip: "Headphones disconnected").
   - Run `InitializeConnectedDevices()` to populate known connected roots (so we don't treat already-present virtual endpoints as arrivals).
   - Run `CheckAndNotifyNow()` which:
     - Calls `GetHeadphonesBatteryLevel()` to find a matching `WH-1000XM3` and read battery via `CM_Get_DevNode_PropertyW`.
     - Calls `IsAnyHeadsetConnected()` (Bluetooth scan) to ensure the device is actually connected before showing a startup balloon.
   - Update the tray tooltip once based on `IsAnyHeadsetConnected()` (authoritative) and `g_connectedDevices` as a fallback.
   - Register for device interface notifications (RegisterDeviceNotificationW) and enter the message loop.

2. On device notifications:
   - `WindowProc` handles `WM_DEVICECHANGE` for `DBT_DEVICEARRIVAL` and `DBT_DEVNODES_CHANGED`.
   - For `DBT_DEVICEARRIVAL` with `DBT_DEVTYP_DEVICEINTERFACE`, `HandleDeviceInterfaceArrival()` is called.
   - `HandleDeviceInterfaceArrival()` attempts:
     - A quick Bluetooth-verified re-check (calling `InitializeConnectedDevices()`) to detect true BT-connected headphones.
     - A mapping from the incoming `PDEV_BROADCAST_DEVICEINTERFACE_W` to an interface via `SetupDiEnumDeviceInterfaces` + `SetupDiGetDeviceInterfaceDetailW`; compares device paths (case-insensitive / substring tolerant) to find a matching devnode.
     - If mapping fails, falls back to enumerating devices and uses `ReadBatteryFromDevInst()` to collect battery info.
     - Compares the discovered set with the known `g_connectedDevices` set to detect additions/removals and show balloons and update the tooltip accordingly.
   - A debounced asynchronous re-check thread (`ArrivalRecheckThread`) is also launched to catch Bluetooth state changes that might be reported slightly after interface notifications.

Required behavior (what the program is supposed to do)

- Always create a tray icon at startup.
- If headphones are already powered on and connected at the moment the app starts, the app should:
  - Show a balloon that the headphones are connected and display battery level.
  - Set the tray tooltip to `Headphones connected`.
- If headphones are off at startup, the app should set tooltip to `Headphones disconnected` and not show the startup balloon.
- When headphones connect / disconnect while the app is running, show appropriate balloon and update tooltip.
- Avoid duplicate or noisy notifications (15-second duplicate suppression, throttling devnodes changes).

Features that currently work (confirmed during conversation)

- The tray icon is added at startup and persists in the notification area.
- A startup check is performed that finds `WH-1000XM3` and shows the balloon when they are connected at startup.
- Battery-level reading via `GetHeadphonesBatteryLevel()` can find the device and format a battery string when the shared battery property is available.
- Device arrival path is logged and `HandleDeviceInterfaceArrival()` runs on interface notifications (lots of debug messages observed).
- Re-check thread and Bluetooth scan (`IsAnyHeadsetConnected()`) are implemented and can trigger a balloon when Bluetooth reports `fConnected`.
- Build errors introduced during edits were fixed and the project builds successfully.

Features not yet fully confirmed or that remain flaky

- Tooltip visual refresh: although the code updates `g_nid.szTip` and calls `Shell_NotifyIconW(NIM_MODIFY, &g_nid)`, Windows shell may not immediately repaint the visible tooltip — it sometimes requires hover/refresh. We added authoritative startup logic to set the tooltip based on Bluetooth state, but shell caching may still make changes not visible until hover.
- Precise mapping from `PDEV_BROADCAST_DEVICEINTERFACE_W::dbcc_name` to the devnode/interface detail: we improved matching using `SetupDiGetDeviceInterfaceDetailW` and case-insensitive substring matching, but there may still be stacks/drivers where the incoming interface path differs and mapping fails (fallback enumeration handles that case).
- Some device-interface notifications are noisy (many interface paths reported). The re-check thread and Bluetooth-verified scanning mitigate this, but the arrival detection may still miss some edge cases depending on timing of the Bluetooth stack.

List of code changes and fixes tried in this session

- Added tray icon installation at startup (so icon appears even when headphones are disconnected).
- Added startup tooltip default `Headphones disconnected` and later update logic based on Bluetooth scan.
- Implemented `GetHeadphonesBatteryLevel()` using `SetupDiGetClassDevs` + `CM_Get_DevNode_PropertyW` with upward devnode walk.
- Implemented `InitializeConnectedDevices()` to populate `g_connectedDevices` from present `WH-1000XM3` devnodes and verify connection using Bluetooth APIs and device root parsing.
- Added `HandleDeviceInterfaceArrival()` to process `DBT_DEVTYP_DEVICEINTERFACE` notifications, enumerating interfaces and attempting to map the notification path to an interface detail.
- Implemented `pathMatches()` helper to compare device paths case-insensitively and allow substring matches to increase tolerance of device path differences.
- Added an Arrival re-check thread `ArrivalRecheckThread` (debounced) to perform a delayed `InitializeConnectedDevices()` when interface notifications arrive rapidly; shows balloon and updates tooltip if new connection appears.
- Added `IsAnyHeadsetConnected()` Bluetooth helper to scan paired/remembered devices and check `fConnected` for `WH-1000XM3`.
- Ensured we only show the startup balloon if Bluetooth reports device as connected (prevents false startup balloons for PnP-only presence).
- Added tooltip updates on arrival/removal events and at startup; replaced incorrect `wcsncpy_s` usage with `wcscpy_s` and corrected buffer sizes.
- Debug logs added at many points to trace interface notifications, mapping attempts, `InitializeConnectedDevices` results, and Bluetooth scan results.
- Fixed multiple build issues that arose during iterative edits (duplicate globals, incorrect API usage, missing forward declarations, thread-function pointer conversion) and re-ordered declarations where needed.

Notes and next steps

- If the tooltip still appears stale, a forced visual refresh can be implemented: remove & re-add the tray icon on state change (NIM_DELETE followed by NIM_ADD) or change the icon handle during NIM_MODIFY to force the shell to repaint.
- If arrival detection remains flaky on your setup, provide the debug OutputDebugString logs for a connect sequence (the `dbcc_name` lines and the new debug messages). With those we can refine path-matching heuristics or adjust the timing/settle delay to match your Bluetooth stack's behavior.

-- End of conversation summary

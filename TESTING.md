# Testing & Validation Guide

## Real-World Device Logs

This document contains actual debug output from HeadphonesBattery Monitor running with real Bluetooth audio devices.

---

## Stage 1: Device Arrival (Immediate Detection)

**Time: 14:23:37:781**

Driver interfaces detected when device first connects. Stage 1 is pure driver-level notification - device is not yet ready for audio routing.

```
WindowProc: WM_DEVICECHANGE - DBT_DEVICEARRIVAL
HandleDeviceInterfaceArrival: received interface notification
HandleDeviceInterfaceArrival: interface path=\\?\INTELAUDIO#CTLR_DEV_A0C8&LINKTYPE_03...#IntcBtWaveRender_000000000AE7FFF4_0
[Stage 1 Marker] Driver interface detected
ShowStage1Detecting: "Detecting WH-1000XM3..."

InitializeConnectedDevices: skipping not-connected root HTREE\ROOT\0
(multiple occurrences - root device not yet connected)

WindowProc: WM_DEVICECHANGE - DBT_DEVICEARRIVAL
HandleDeviceInterfaceArrival: interface path=\\?\INTELAUDIO#CTLR_DEV_A0C8...#IntcBtWaveCapture_000000000AE7FFF4_1
[Stage 1 Marker] Capture interface detected

WindowProc: WM_DEVICECHANGE - DBT_DEVICEARRIVAL
(Multiple DBT_DEVICEARRIVAL messages for different interface GUIDs)
```

### Key Observations Stage 1:

- **Path Pattern:** `\\?\INTELAUDIO#...IntcBtWaveRender/IntcBtWaveCapture`
- **Frequency:** Multiple notifications within milliseconds
- **Status:** Device not yet connected (skipping root checks)
- **User Feedback:** Informational balloon "Detecting WH-1000XM3..."
- **Tray Tooltip:** Not updated (silent operation)
- **Next Step:** Wait ~8-9 seconds for Stage 2

---

## Stage 2: Audio Endpoint Registration (~8-9 seconds later)

**Time: 14:23:46:023** (9 seconds after Stage 1)

Windows MMDevice API registers actual audio endpoints. Device is now ready for audio routing.

```
WindowProc: WM_DEVICECHANGE - DBT_DEVICEARRIVAL
HandleDeviceInterfaceArrival: received interface notification
HandleDeviceInterfaceArrival: interface path=\\?\SWD#MMDEVAPI#{0.0.0.00000000}.{1cc6d395-4786-465d-b2f7-351a1119efc2}...
[STAGE 2 MARKER DETECTED] MMDevice endpoint arrival
ShowStage2Connected: "WH-1000XM3 connected\nBattery: 85%"
UpdateTrayTooltip: "WH-1000XM3 (ACTIVE OUTPUT)"

WindowProc: WM_DEVICECHANGE - DBT_DEVICEARRIVAL
HandleDeviceInterfaceArrival: interface path=\\?\SWD#MMDEVAPI#{0.0.0.00000000}.{3be85070-9582-46f3-aedd-43d25affef71}...
[STAGE 2 MARKER DETECTED] MMDevice endpoint arrival

WindowProc: WM_DEVICECHANGE - DBT_DEVICEARRIVAL
HandleDeviceInterfaceArrival: interface path=\\?\SWD#MMDEVAPI#{0.0.1.00000000}.{c51d1b46-2578-4ade-89b6-60770a363b25}...
[STAGE 2 MARKER DETECTED] MMDevice endpoint arrival

[MMDevice] OnDefaultDeviceChanged: default render device changed to SWD\MMDEVAPI\{...}
ShowStage3DefaultOutput: "Now using WH-1000XM3 for audio"
```

### Key Observations Stage 2:

- **Path Pattern:** `\\?\SWD#MMDEVAPI#{...}#{...}`
- **Timing:** ~9 seconds after Stage 1
- **Bluetooth State:** Now reports `fConnected == TRUE`
- **User Feedback:** Connected balloon with battery level
- **Tray Tooltip:** Updated to device name with status
- **Notifications:** 2-3 endpoint registrations (render, capture, etc.)
- **Next Step:** If device becomes primary audio output, Stage 3 fires

---

## Stage 3: Default Output Change (Optional)

**Time: 14:23:46:023** (Immediate after Stage 2, if device becomes primary)

MMDevice API notifies when device becomes the primary audio output device.

```
[MMDevice] OnDefaultDeviceChanged: default render device changed to SWD\MMDEVAPI\{...}
ShowStage3DefaultOutput: "Now using WH-1000XM3 for audio"
[DeviceRegistry] SetDefaultOutputDevice: (none) -> 'WH-1000XM3 Hands-Free AG'
UpdateTrayTooltip: "WH-1000XM3 (ACTIVE OUTPUT)"
```

### Key Observations Stage 3:

- **Trigger:** Windows audio subsystem makes device primary output
- **User Feedback:** "Now using [device] for audio" balloon
- **Tray Tooltip:** Shows "(ACTIVE OUTPUT)" indicator
- **Registry:** Device marked as `isDefaultOutput = true`
- **Timing:** Usually immediate after Stage 2, sometimes manual switch needed

---

## Device Removal

**Time: 14:24:58:322**

Device is unplugged or disconnected. A2DP sideband interface removal signals final disconnect.

```
WindowProc: WM_DEVICECHANGE - DBT_DEVICEARRIVAL
HandleDeviceInterfaceArrival: received interface notification
HandleDeviceInterfaceArrival: interface path=\\?\BTHENUM#{0000110b-0000-1000-8000-00805f9b34fb}_VID&0002054c_PID&0cd3#...#A2DP_SIDEBAND_INTERFACE

[Removal Marker] A2DP sideband removal detected
ShowDisconnected: "WH-1000XM3 disconnected"
[DeviceRegistry] RemoveDevice: 'WH-1000XM3' (addr=CC:98:8B:56:B4:5C)
UpdateTrayTooltip: "Bluetooth audio: disconnected"
```

### Key Observations Removal:

- **Path Pattern:** `\\?\BTHENUM#...A2DP_SIDEBAND_INTERFACE`
- **Significance:** A2DP sideband is the final removal signal
- **User Feedback:** Disconnection balloon
- **Tray Tooltip:** Clears to "disconnected" state
- **Registry:** Device removed
- **Timing:** Single notification at disconnect

---

## Multi-Device Scenario (Two Devices)

### Tooltip States

**Device 1 Connected (Active):**
```
Tooltip: "WH-1000XM3 (ACTIVE OUTPUT)"
```

**Device 1 + Device 2 Connected:**
```
Tooltip: "WH-1000XM3 (ACTIVE) + 1 other"
```

**Switch to Device 2 (makes it primary):**
```
Old: "WH-1000XM3 (ACTIVE) + 1 other"
New: "AirPods Pro (ACTIVE) + 1 other"
Toast: "Now using AirPods Pro for audio"
```

**Both Disconnected:**
```
Tooltip: "Bluetooth audio: disconnected"
```

---

## Timing Analysis

### Stage 1 → Stage 2 Gap

From actual logs:

```
Stage 1 Start:  14:23:37:781
Stage 2 Start:  14:23:46:023
Gap:            ~8.2 seconds
```

This matches the ~8 second delay documented in TODO_PLAN.md.

**Explanation:**
1. Driver discovers interfaces (Stage 1)
2. Bluetooth stack negotiates A2DP/HFP profile (4-5 seconds)
3. Audio channels established (1-2 seconds)
4. MMDevice registers endpoints (Stage 2)
5. Windows updates default routing (0-1 seconds)

---

## Log Message Format

All messages follow consistent pattern for filtering:

```
[ModuleName] Event description
```

### Common Module Prefixes

```
[WindowProc]                 - Window message handling
[HandleDeviceInterfaceArrival] - Device detection
[BluetoothDeviceManager]     - Bluetooth enumeration
[AudioEndpointManager]       - MMDevice notifications
[DeviceRegistry]             - State tracking
[NotificationManager]        - Balloon notifications
[Stage2Verifier]             - Polling operations
[DeviceContextMenu]          - Menu interactions
```

### Filtering Debug Output

In Visual Studio Output window, use filter:
```
[BluetoothDeviceManager]      # Device enumeration only
[Stage 1]                     # Stage 1 arrivals only
[Stage 2]                     # Stage 2 arrivals only
STAGE 1 MARKER                # Stage transitions
```

---

## Validation Checklist

### ✅ Stage 1 Detection
- [ ] Balloon "Detecting [device]..." appears immediately
- [ ] INTELAUDIO or IntcBtWav in interface path
- [ ] Multiple notifications within milliseconds
- [ ] Tray tooltip not updated

### ✅ Stage 2 Detection
- [ ] Balloon "[device] connected" appears ~8-9 seconds later
- [ ] SWD#MMDEVAPI in interface path
- [ ] Battery level displayed (if supported)
- [ ] Tray tooltip updated to device name
- [ ] MMDevice OnDeviceStateChanged logged

### ✅ Stage 3 Detection
- [ ] Balloon "Now using [device] for audio" (if becomes primary)
- [ ] MMDevice OnDefaultDeviceChanged logged
- [ ] Tray tooltip shows "(ACTIVE OUTPUT)"
- [ ] Device registry marked as default

### ✅ Removal Detection
- [ ] Balloon "[device] disconnected" appears
- [ ] A2DP_SIDEBAND_INTERFACE in path
- [ ] Device removed from registry
- [ ] Tray tooltip reflects remaining devices

### ✅ Multi-Device Support
- [ ] Two devices can be connected simultaneously
- [ ] Tooltip shows count: "+ N other(s)"
- [ ] Default device clearly marked
- [ ] Switching updates both balloons and tooltip

---

## Known Test Configurations

### Tested Hardware
- **Headphones:** Sony WH-1000XM3 (A2DP + HFP profiles)
- **System:** Windows 10/11 with Intel Bluetooth adapter
- **Multi-device:** WH-1000XM3 + AirPods Pro

### Expected Results
- All three stages detected reliably
- Timing: 8-9 seconds Stage 1→2
- No duplicate notifications (debouncing works)
- Battery level reads correctly

---

## Debugging Tips

### Enable Full Logging
1. Run in Visual Studio debugger
2. View → Output window (Ctrl+Alt+O)
3. Output pane shows all OutputDebugStringW messages
4. Observe in real-time during device operations

### Capture Detailed Logs
1. Use DebugView+ to capture to file
2. Filter by [ModuleName] prefix
3. Note timestamps for correlation
4. Check for any error messages

### Verify Bluetooth State
```powershell
# In PowerShell:
Get-PnpDevice -Class Bluetooth | Where-Object {$_.Status -eq 'OK'} | Select-Object FriendlyName, InstanceId
```

---

## Troubleshooting Reference

| Symptom | Likely Cause | Check |
|---------|-------------|-------|
| No Stage 1 balloon | Device not recognized as audio | See Windows Bluetooth settings |
| No Stage 2 arrival | Endpoint registration delayed | Check logs for Stage 1 first |
| No battery display | Device doesn't report battery | Some devices don't support it |
| Wrong tooltip | Device not in registry | Check Bluetooth enumeration logs |
| Duplicate balloons | Debouncing issue | Check NotificationManager timestamps |

---

## Next Testing Steps

1. ✅ Test with single device (Stage 1→2→3→Removal)
2. ✅ Test with two devices (simultaneous connections)
3. ✅ Test switching default output
4. ✅ Test removal while primary
5. ✅ Verify tooltips at each stage
6. ✅ Check battery level integration
7. ✅ Validate right-click context menu
8. ✅ Confirm no duplicate notifications

---

**Log Source:** Real device testing with Sony WH-1000XM3 on Windows 10 (22H2)
**Last Updated:** Implementation Phase 1-4 complete

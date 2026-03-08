/*
 * NotificationManager.h
 * 
 * MODULE: Notification Manager (Three-Stage Notifications)
 * PURPOSE: Manages three-stage device lifecycle notifications and tray tooltip updates
 * 
 * DESCRIPTION:
 *   Handles user-facing notifications for Bluetooth audio device events across three
 *   distinct stages of device lifecycle, plus disconnect notifications.
 * 
 *   Stage 1: "Detecting [device]..." - Driver interfaces detected, device not yet ready
 *   Stage 2: "[device] connected [Battery: XX%]" - Audio endpoints ready, tray updates
 *   Stage 3: "Now using [device] for audio" - Device becomes primary output
 *   Disconnect: "[device] disconnected" - Device removed
 * 
 *   Also manages smart tray tooltip formatting:
 *   - Single device: "[Name] (ACTIVE OUTPUT)" or "[Name] (standby)"
 *   - Multiple devices: "[Primary] (ACTIVE) + N other(s)"
 *   - No devices: "Bluetooth audio: disconnected"
 * 
 * USAGE:
 *   1. Global instance: extern NotificationManager* g_notificationManager (created in main)
 *   2. Call stage-specific methods:
 *      - g_notificationManager->ShowStage1Detecting(name)
 *      - g_notificationManager->ShowStage2Connected(name, battery)
 *      - g_notificationManager->ShowStage3DefaultOutput(name)
 *      - g_notificationManager->ShowDisconnected(name)
 *   3. Tooltip automatically updated at stage 2 and 3
 * 
 * KEY METHODS:
 *   - ShowStage1Detecting() - Show "Detecting..." balloon (silent, no tooltip update)
 *   - ShowStage2Connected() - Show "connected" balloon, update tooltip
 *   - ShowStage3DefaultOutput() - Show "now using" balloon, add active indicator
 *   - ShowDisconnected() - Show "disconnected" balloon, update tooltip
 *   - UpdateTrayTooltip() - Synchronize tooltip with current device state
 *   - GetCurrentTooltip() - Get formatted tooltip string
 * 
 * FEATURES:
 *   - Debouncing: Prevents duplicate notifications within 1-second window
 *   - Smart Formatting: Tooltip adapts to single/multi-device scenarios
 *   - Battery Integration: Displays battery level when available
 *   - Stage-Aware: Different messages for each stage
 * 
 * INTEGRATION:
 *   - Called by: HandleDeviceInterfaceArrival(), AudioEndpointManager callbacks
 *   - Uses: DeviceRegistry for device information, TrayNotification for UI updates
 *   - Outputs: Debug logging and user-facing notifications
 */

#pragma once

#include <windows.h>
#include <string>
#include "DeviceRegistry.h"

// Notification manager for three-stage balloon system
class NotificationManager {
public:
    NotificationManager();
    ~NotificationManager();

    // Show Stage 1 notification (driver interfaces detected)
    void ShowStage1Detecting(const std::wstring& deviceName);

    // Show Stage 2 notification (audio endpoint ready)
    void ShowStage2Connected(const std::wstring& deviceName, BYTE batteryLevel = 0);

    // Show Stage 3 notification (became primary output)
    void ShowStage3DefaultOutput(const std::wstring& deviceName);

    // Show disconnection notification
    void ShowDisconnected(const std::wstring& deviceName);

    // Update tray tooltip based on registry state
    void UpdateTrayTooltip();

    // Get formatted tooltip for current state
    std::wstring GetCurrentTooltip() const;

private:
    // Format tooltip for single device
    std::wstring FormatSingleDeviceTooltip(const BluetoothAudioDevice& device) const;

    // Format tooltip for multiple devices
    std::wstring FormatMultiDeviceTooltip() const;

    // Show balloon notification with title and message
    void ShowBalloon(const std::wstring& title, const std::wstring& message);

    // Debounce state to avoid duplicate notifications
    ULONGLONG m_lastNotifyTime;
    std::wstring m_lastNotifyMessage;
};

// Global notification manager instance
extern NotificationManager* g_notificationManager;

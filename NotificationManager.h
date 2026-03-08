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

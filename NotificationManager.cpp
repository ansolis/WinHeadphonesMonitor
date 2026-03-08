#include "NotificationManager.h"
#include "TrayNotification.h"
#include <sstream>

// Global instance
NotificationManager* g_notificationManager = nullptr;

NotificationManager::NotificationManager()
    : m_lastNotifyTime(0)
{
    OutputDebugStringW(L"[NotificationManager] Initialized\n");
}

NotificationManager::~NotificationManager()
{
}

void NotificationManager::ShowStage1Detecting(const std::wstring& deviceName)
{
    std::wstring title = L"Detecting...";
    std::wstring message = deviceName + L"\n\nInitializing audio profile...";
    
    wchar_t logMsg[512];
    _snwprintf_s(logMsg, _countof(logMsg), _TRUNCATE,
        L"[NotificationManager] STAGE 1: Detecting '%s'\n", deviceName.c_str());
    OutputDebugStringW(logMsg);
    
    ShowBalloon(title, message);
}

void NotificationManager::ShowStage2Connected(const std::wstring& deviceName, BYTE batteryLevel)
{
    std::wstring title = L"Connected";
    std::wstring message = deviceName;
    
    if (batteryLevel > 0) {
        wchar_t batteryBuf[64];
        _snwprintf_s(batteryBuf, _countof(batteryBuf), _TRUNCATE,
            L"\n\nBattery: %d%%", batteryLevel);
        message += batteryBuf;
    }

    wchar_t logMsg[512];
    _snwprintf_s(logMsg, _countof(logMsg), _TRUNCATE,
        L"[NotificationManager] STAGE 2: Connected '%s' battery=%d%%\n", 
        deviceName.c_str(), batteryLevel);
    OutputDebugStringW(logMsg);
    
    ShowBalloon(title, message);
    
    // Update tray tooltip only at stage 2
    UpdateTrayTooltip();
}

void NotificationManager::ShowStage3DefaultOutput(const std::wstring& deviceName)
{
    std::wstring title = L"Active Output";
    std::wstring message = L"Now using " + deviceName + L" for audio";

    wchar_t logMsg[512];
    _snwprintf_s(logMsg, _countof(logMsg), _TRUNCATE,
        L"[NotificationManager] STAGE 3: Default Output '%s'\n", deviceName.c_str());
    OutputDebugStringW(logMsg);
    
    ShowBalloon(title, message);
    
    // Update tray tooltip at stage 3
    UpdateTrayTooltip();
}

void NotificationManager::ShowDisconnected(const std::wstring& deviceName)
{
    std::wstring title = L"Disconnected";
    std::wstring message = deviceName + L" disconnected";

    wchar_t logMsg[512];
    _snwprintf_s(logMsg, _countof(logMsg), _TRUNCATE,
        L"[NotificationManager] DISCONNECTED: '%s'\n", deviceName.c_str());
    OutputDebugStringW(logMsg);
    
    ShowBalloon(title, message);
    
    // Update tray tooltip on disconnection
    UpdateTrayTooltip();
}

std::wstring NotificationManager::FormatSingleDeviceTooltip(const BluetoothAudioDevice& device) const
{
    std::wstring tooltip = device.friendlyName;
    if (device.isDefaultOutput) {
        tooltip += L" (ACTIVE OUTPUT)";
    } else {
        tooltip += L" (standby)";
    }
    return tooltip;
}

std::wstring NotificationManager::FormatMultiDeviceTooltip() const
{
    if (!g_deviceRegistry) {
        return L"Headphones";
    }

    std::wstring tooltip;
    int connectedCount = g_deviceRegistry->GetConnectedDeviceCount();
    
    if (connectedCount == 0) {
        return L"Bluetooth audio: disconnected";
    }

    // Find default output device
    auto defaultDevice = g_deviceRegistry->GetDefaultOutputDevice();
    if (defaultDevice) {
        tooltip = defaultDevice->friendlyName;
        tooltip += L" (ACTIVE)";
    }

    int otherCount = connectedCount - (defaultDevice ? 1 : 0);
    if (otherCount > 0) {
        wchar_t otherStr[64];
        _snwprintf_s(otherStr, _countof(otherStr), _TRUNCATE,
            L" + %d other%s", otherCount, otherCount == 1 ? L"" : L"s");
        tooltip += otherStr;
    }

    return tooltip;
}

std::wstring NotificationManager::GetCurrentTooltip() const
{
    if (!g_deviceRegistry) {
        return L"Headphones";
    }

    int count = g_deviceRegistry->GetConnectedDeviceCount();
    if (count == 0) {
        return L"Bluetooth audio: disconnected";
    } else if (count == 1) {
        auto device = g_deviceRegistry->GetDefaultOutputDevice();
        if (device) {
            return FormatSingleDeviceTooltip(*device);
        }
    }

    return FormatMultiDeviceTooltip();
}

void NotificationManager::UpdateTrayTooltip()
{
    std::wstring tooltip = GetCurrentTooltip();

    wchar_t logMsg[512];
    _snwprintf_s(logMsg, _countof(logMsg), _TRUNCATE,
        L"[NotificationManager] UpdateTrayTooltip: '%s'\n", tooltip.c_str());
    OutputDebugStringW(logMsg);

    // Call the global TrayNotification function
    ::UpdateTrayTooltip(tooltip);
}

void NotificationManager::ShowBalloon(const std::wstring& title, const std::wstring& message)
{
    ULONGLONG now = GetTickCount64();

    // Debounce identical messages within 1 second
    if (message == m_lastNotifyMessage && (now - m_lastNotifyTime) < 1000) {
        OutputDebugStringW(L"[NotificationManager] Suppressing duplicate balloon\n");
        return;
    }

    m_lastNotifyTime = now;
    m_lastNotifyMessage = message;

    ShowBalloonNotification(title, message);
}

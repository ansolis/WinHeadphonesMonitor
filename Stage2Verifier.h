#pragma once

#include <windows.h>
#include <string>

// Stage 2 polling verifier for robust endpoint detection
class Stage2Verifier {
public:
    Stage2Verifier();
    ~Stage2Verifier();

    // Start polling for stage 2 readiness after stage 1 detection
    // Polls every 300ms for up to 10 seconds
    void StartPolling(const std::wstring& deviceName, const std::wstring& bluetoothAddress);

    // Stop polling
    void StopPolling();

    // Check if device meets stage 2 criteria:
    // - Battery property readable OR
    // - Bluetooth fConnected == TRUE OR
    // - CM_Get_DevNode_Status shows DN_STARTED OR
    // - SWD#MMDEVAPI in recent notifications
    bool IsStage2Ready(const std::wstring& bluetoothAddress);

private:
    static DWORD WINAPI PollingThreadProc(LPVOID param);
    
    HANDLE m_pollThread;
    bool m_polling;
    std::wstring m_deviceName;
    std::wstring m_bluetoothAddress;
    ULONGLONG m_pollStartTime;
};

// Global instance
extern Stage2Verifier* g_stage2Verifier;

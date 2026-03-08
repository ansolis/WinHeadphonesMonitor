/*
 * Stage2Verifier.h
 * 
 * MODULE: Stage 2 Verification (Optional Feature)
 * PURPOSE: Provides robust device readiness detection through timeout-based polling
 * 
 * DESCRIPTION:
 *   When a Bluetooth device first arrives (Stage 1), the driver-level interfaces are detected
 *   but the device is not yet ready for audio routing. This module implements a polling mechanism
 *   to detect when the device reaches Stage 2 (audio endpoints registered and ready).
 * 
 *   Spawns a dedicated polling thread that runs for up to 10 seconds, checking every 300ms
 *   for stage 2 readiness criteria (battery readable, fConnected==TRUE, DN_STARTED status, etc.)
 * 
 * USAGE:
 *   1. Create global instance: Stage2Verifier g_stage2Verifier
 *   2. After Stage 1 detection in HandleDeviceInterfaceArrival():
 *      g_stage2Verifier->StartPolling(deviceName, bluetoothAddress)
 *   3. Polling runs in background thread, logs when Stage 2 is confirmed
 *   4. Automatically exits when ready or timeout reached
 * 
 * KEY METHODS:
 *   - StartPolling(deviceName, address) - Begin polling for this device
 *   - StopPolling() - Force stop polling (called on cleanup)
 *   - IsStage2Ready(address) - Check if device meets stage 2 criteria
 * 
 * INTEGRATION:
 *   - Called by: HandleDeviceInterfaceArrival() when Stage 1 detected
 *   - Uses: DeviceRegistry for device state checking
 *   - Outputs: Debug logging with timestamps and elapsed time
 * 
 * THREAD SAFETY:
 *   - Polling runs on dedicated thread created with CreateThread()
 *   - m_polling flag signals thread to exit
 *   - WaitForSingleObject() ensures clean shutdown
 */

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

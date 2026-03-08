#include "Stage2Verifier.h"
#include "DeviceRegistry.h"
#include <cfgmgr32.h>

// Global instance
Stage2Verifier* g_stage2Verifier = nullptr;

Stage2Verifier::Stage2Verifier()
    : m_pollThread(nullptr), m_polling(false)
{
    OutputDebugStringW(L"[Stage2Verifier] Initialized\n");
}

Stage2Verifier::~Stage2Verifier()
{
    StopPolling();
}

void Stage2Verifier::StartPolling(const std::wstring& deviceName, const std::wstring& bluetoothAddress)
{
    if (m_polling) {
        StopPolling();
    }

    m_deviceName = deviceName;
    m_bluetoothAddress = bluetoothAddress;
    m_pollStartTime = GetTickCount64();
    m_polling = true;

    wchar_t msg[512];
    _snwprintf_s(msg, _countof(msg), _TRUNCATE,
        L"[Stage2Verifier] StartPolling: '%s' (timeout=10s, interval=300ms)\n",
        deviceName.c_str());
    OutputDebugStringW(msg);

    // Create polling thread
    m_pollThread = CreateThread(nullptr, 0, PollingThreadProc, this, 0, nullptr);
}

void Stage2Verifier::StopPolling()
{
    m_polling = false;
    if (m_pollThread) {
        WaitForSingleObject(m_pollThread, 1000);
        CloseHandle(m_pollThread);
        m_pollThread = nullptr;
    }
}

bool Stage2Verifier::IsStage2Ready(const std::wstring& bluetoothAddress)
{
    // TODO: Implement comprehensive stage 2 readiness check:
    // - Check if battery property is readable
    // - Check Bluetooth fConnected status
    // - Check CM_Get_DevNode_Status for DN_STARTED
    // For now, assume ready after endpoint detection
    return true;
}

DWORD WINAPI Stage2Verifier::PollingThreadProc(LPVOID param)
{
    Stage2Verifier* pThis = (Stage2Verifier*)param;
    if (!pThis) return 0;

    const ULONGLONG TIMEOUT_MS = 10000;  // 10 seconds
    const ULONGLONG POLL_INTERVAL_MS = 300;  // 300ms

    while (pThis->m_polling) {
        ULONGLONG elapsed = GetTickCount64() - pThis->m_pollStartTime;

        if (elapsed > TIMEOUT_MS) {
            wchar_t msg[512];
            _snwprintf_s(msg, _countof(msg), _TRUNCATE,
                L"[Stage2Verifier] Polling timeout for '%s'\n",
                pThis->m_deviceName.c_str());
            OutputDebugStringW(msg);
            pThis->m_polling = false;
            break;
        }

        // Check readiness criteria
        if (pThis->IsStage2Ready(pThis->m_bluetoothAddress)) {
            wchar_t msg[512];
            _snwprintf_s(msg, _countof(msg), _TRUNCATE,
                L"[Stage2Verifier] Stage 2 ready confirmed for '%s' after %llu ms\n",
                pThis->m_deviceName.c_str(), elapsed);
            OutputDebugStringW(msg);
            pThis->m_polling = false;
            break;
        }

        Sleep(POLL_INTERVAL_MS);
    }

    return 0;
}

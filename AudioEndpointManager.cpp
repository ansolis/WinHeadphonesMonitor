#include "AudioEndpointManager.h"
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <propvarutil.h>
#include <propkey.h>
#include <wrl/client.h>
#include <sstream>

using Microsoft::WRL::ComPtr;

// Global instance
AudioEndpointManager* g_audioEndpointManager = nullptr;

// IMMNotificationClient implementation for detecting audio endpoint changes
class AudioNotificationClient : public IMMNotificationClient {
private:
    LONG m_cRef;

public:
    AudioNotificationClient() : m_cRef(1) {}

    // IUnknown methods
    ULONG STDMETHODCALLTYPE AddRef() {
        return InterlockedIncrement(&m_cRef);
    }

    ULONG STDMETHODCALLTYPE Release() {
        LONG lRef = InterlockedDecrement(&m_cRef);
        if (lRef == 0) {
            delete this;
        }
        return lRef;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppUnk) {
        if (iid == IID_IUnknown) {
            *ppUnk = (IUnknown*)this;
        } else if (iid == __uuidof(IMMNotificationClient)) {
            *ppUnk = (IMMNotificationClient*)this;
        } else {
            *ppUnk = nullptr;
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }

    // IMMNotificationClient methods
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId) {
        if (flow == eRender && role == eConsole) {
            std::wstring msg = L"[MMDevice] OnDefaultDeviceChanged: default render device changed to ";
            if (pwstrDefaultDeviceId) {
                msg += pwstrDefaultDeviceId;
            } else {
                msg += L"(null)";
            }
            msg += L"\n";
            OutputDebugStringW(msg.c_str());
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR pwstrDeviceId) {
        std::wstring msg = L"[MMDevice] OnDeviceAdded: ";
        if (pwstrDeviceId) {
            msg += pwstrDeviceId;
        } else {
            msg += L"(null)";
        }
        msg += L"\n";
        OutputDebugStringW(msg.c_str());
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR pwstrDeviceId) {
        std::wstring msg = L"[MMDevice] OnDeviceRemoved: ";
        if (pwstrDeviceId) {
            msg += pwstrDeviceId;
        } else {
            msg += L"(null)";
        }
        msg += L"\n";
        OutputDebugStringW(msg.c_str());
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState) {
        std::wstring msg = L"[MMDevice] OnDeviceStateChanged: ";
        if (pwstrDeviceId) {
            msg += pwstrDeviceId;
        } else {
            msg += L"(null)";
        }
        wchar_t stateBuf[256];
        _snwprintf_s(stateBuf, _countof(stateBuf), _TRUNCATE, L" state=0x%08X\n", dwNewState);
        msg += stateBuf;
        OutputDebugStringW(msg.c_str());
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key) {
        // Not logging every property change to avoid noise
        return S_OK;
    }
};

AudioEndpointManager::AudioEndpointManager()
    : m_pEnumerator(nullptr), m_pClient(nullptr), m_initialized(false)
{
}

AudioEndpointManager::~AudioEndpointManager()
{
    Shutdown();
}

bool AudioEndpointManager::Initialize()
{
    if (m_initialized) {
        return true;
    }

    OutputDebugStringW(L"[AudioEndpointManager] Initializing...\n");

    // Create MMDevice enumerator
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&m_pEnumerator
    );

    if (FAILED(hr)) {
        wchar_t buf[256];
        _snwprintf_s(buf, _countof(buf), _TRUNCATE, L"[AudioEndpointManager] CoCreateInstance failed: 0x%08X\n", hr);
        OutputDebugStringW(buf);
        return false;
    }

    // Create and register notification client
    m_pClient = new AudioNotificationClient();
    if (!m_pClient) {
        if (m_pEnumerator) {
            m_pEnumerator->Release();
            m_pEnumerator = nullptr;
        }
        OutputDebugStringW(L"[AudioEndpointManager] Failed to allocate notification client\n");
        return false;
    }

    hr = m_pEnumerator->RegisterEndpointNotificationCallback(m_pClient);
    if (FAILED(hr)) {
        wchar_t buf[256];
        _snwprintf_s(buf, _countof(buf), _TRUNCATE, L"[AudioEndpointManager] RegisterEndpointNotificationCallback failed: 0x%08X\n", hr);
        OutputDebugStringW(buf);
        m_pClient->Release();
        m_pClient = nullptr;
        if (m_pEnumerator) {
            m_pEnumerator->Release();
            m_pEnumerator = nullptr;
        }
        return false;
    }

    m_initialized = true;
    OutputDebugStringW(L"[AudioEndpointManager] Initialized successfully\n");
    return true;
}

void AudioEndpointManager::Shutdown()
{
    OutputDebugStringW(L"[AudioEndpointManager] Shutting down...\n");

    if (m_pEnumerator && m_pClient) {
        m_pEnumerator->UnregisterEndpointNotificationCallback(m_pClient);
    }

    if (m_pClient) {
        m_pClient->Release();
        m_pClient = nullptr;
    }

    if (m_pEnumerator) {
        m_pEnumerator->Release();
        m_pEnumerator = nullptr;
    }

    m_initialized = false;
}

bool AudioEndpointManager::IsAudioEndpointRegistered(const std::wstring& deviceName)
{
    if (!m_pEnumerator) {
        return false;
    }

    // Get all render endpoints
    IMMDeviceCollection* pDevices = nullptr;
    HRESULT hr = m_pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pDevices);
    if (FAILED(hr)) {
        return false;
    }

    UINT count = 0;
    pDevices->GetCount(&count);

    bool found = false;
    // For now, just check if any active render endpoints exist
    // In a full implementation, we would match by device friendly name
    found = (count > 0);

    pDevices->Release();
    return found;
}

std::wstring AudioEndpointManager::GetDefaultAudioEndpointId()
{
    if (!m_pEnumerator) {
        return L"";
    }

    IMMDevice* pDevice = nullptr;
    HRESULT hr = m_pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    if (FAILED(hr)) {
        return L"";
    }

    LPWSTR pszId = nullptr;
    hr = pDevice->GetId(&pszId);
    if (SUCCEEDED(hr)) {
        std::wstring id = pszId;
        CoTaskMemFree(pszId);
        pDevice->Release();
        return id;
    }

    pDevice->Release();
    return L"";
}

bool AudioEndpointManager::IsDefaultAudioEndpoint(const std::wstring& deviceId)
{
    std::wstring defaultId = GetDefaultAudioEndpointId();
    return !defaultId.empty() && defaultId == deviceId;
}

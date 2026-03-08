#pragma once

#include <windows.h>
#include <mmdeviceapi.h>
#include <string>
#include <map>

// Forward declaration
struct BluetoothAudioDevice;

// Audio endpoint listener that detects when Bluetooth audio endpoints are registered
// and when they become the default audio output device (MMDevice notifications)
class AudioEndpointManager {
public:
    AudioEndpointManager();
    ~AudioEndpointManager();

    // Initialize COM and register for MMDevice notifications
    bool Initialize();

    // Cleanup and unregister notifications
    void Shutdown();

    // Check if a given device name/ID matches a known audio endpoint
    // Returns true if the endpoint is found in MMDevice registry
    bool IsAudioEndpointRegistered(const std::wstring& deviceName);

    // Get the device ID of the current default audio output endpoint
    std::wstring GetDefaultAudioEndpointId();

    // Check if a specific device is the current default output
    bool IsDefaultAudioEndpoint(const std::wstring& deviceId);

private:
    IMMDeviceEnumerator* m_pEnumerator;
    class AudioNotificationClient* m_pClient;
    bool m_initialized;
};

// Global instance
extern AudioEndpointManager* g_audioEndpointManager;

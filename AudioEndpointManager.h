/*
 * AudioEndpointManager.h
 * 
 * MODULE: Audio Endpoint Manager (MMDevice Integration)
 * PURPOSE: Integrates with Windows MMDevice API to detect audio endpoint changes
 * 
 * DESCRIPTION:
 *   Implements IMMNotificationClient to receive Windows audio subsystem notifications.
 *   Detects when Bluetooth device endpoints are registered (Stage 2) and when a device
 *   becomes the primary audio output (Stage 3).
 * 
 *   Provides authoritative information about audio routing and device status from the
 *   Windows audio subsystem, complementing Bluetooth API and device notifications.
 * 
 * USAGE:
 *   1. Global instance: extern AudioEndpointManager* g_audioEndpointManager (created in main)
 *   2. Initialize in WinMain(): g_audioEndpointManager->Initialize()
 *   3. Receives callbacks automatically:
 *      - OnDefaultDeviceChanged() - Device becomes primary (Stage 3)
 *      - OnDeviceStateChanged() - Endpoint status changes (Stage 2 marker)
 *      - OnDeviceAdded() - New endpoint added (logging)
 *      - OnDeviceRemoved() - Endpoint removed (logging)
 * 
 * KEY METHODS:
 *   - Initialize() - Register notification client with MMDevice
 *   - OnDefaultDeviceChanged() - Callback for primary device change
 *   - OnDeviceStateChanged() - Callback for endpoint state change
 *   - OnDeviceAdded/OnDeviceRemoved() - Callbacks for endpoint add/remove
 * 
 * WINDOWS AUDIO CONCEPTS:
 *   - MMDevice: Windows multimedia device enumeration and management API
 *   - Endpoint: Audio input/output point (e.g., headphones speakers)
 *   - Default Device: The device Windows audio subsystem uses for app audio
 *   - Device State: DEVICE_STATE_ACTIVE/DISABLED/NOTPRESENT/UNPLUGGED
 * 
 * INTEGRATION:
 *   - Called by: Windows audio subsystem (callbacks are automatic)
 *   - Uses: COM (IUnknown, IMMNotificationClient, IMMDeviceEnumerator)
 *   - Triggers: Notifications through NotificationManager on state changes
 *   - Outputs: Debug logging of all endpoint changes
 * 
 * THREAD SAFETY:
 *   - Callbacks may be invoked from Windows audio subsystem threads
 *   - Should minimize work in callbacks, possibly queue for main thread
 */

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

#pragma once

#include <windows.h>
#include <string>
#include <map>
#include "BluetoothDeviceManager.h"

// Device state enum for tracking device lifecycle
enum DeviceStage {
    STAGE_UNKNOWN,           // Not yet detected
    STAGE1_INTERFACES_DETECTED,  // Driver interfaces found
    STAGE2_ENDPOINT_READY,   // MMDevice endpoints registered
    STAGE3_DEFAULT_OUTPUT,   // Became primary audio output
    STAGE_DISCONNECTED       // Removed
};

// Device registry manager for tracking Bluetooth audio devices
class DeviceRegistry {
public:
    DeviceRegistry();
    ~DeviceRegistry();

    // Add or update device in registry
    void AddOrUpdateDevice(const BluetoothAudioDevice& device);

    // Remove device from registry by address
    void RemoveDevice(const std::wstring& bluetoothAddress);

    // Get device by address
    BluetoothAudioDevice* GetDevice(const std::wstring& bluetoothAddress);

    // Get all connected devices
    const std::map<std::wstring, BluetoothAudioDevice>& GetAllDevices() const;

    // Get count of connected devices
    int GetConnectedDeviceCount() const;

    // Update device stage
    void UpdateDeviceStage(const std::wstring& bluetoothAddress, DeviceStage stage);

    // Get the default audio output device (if any)
    BluetoothAudioDevice* GetDefaultOutputDevice();

    // Update default output device
    void SetDefaultOutputDevice(const std::wstring& bluetoothAddress);

    // Clear all devices
    void Clear();

    // Log all devices
    void LogAllDevices() const;

private:
    std::map<std::wstring, BluetoothAudioDevice> m_devices;
    std::wstring m_defaultOutputAddress;
};

// Global device registry instance
extern DeviceRegistry* g_deviceRegistry;

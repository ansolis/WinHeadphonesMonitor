/*
 * DeviceRegistry.h
 * 
 * MODULE: Device Registry (Multi-Device Support)
 * PURPOSE: Central device state management and tracking for all connected Bluetooth audio devices
 * 
 * DESCRIPTION:
 *   Maintains a registry of all detected Bluetooth audio devices with their current state,
 *   including connection status, default output status, battery level, and device stage.
 *   Provides methods to add, update, remove, and query devices.
 * 
 *   Supports multiple simultaneous Bluetooth audio device connections with per-device
 *   state tracking. Used by all modules that need to know about connected devices.
 * 
 * USAGE:
 *   1. Global instance: extern DeviceRegistry* g_deviceRegistry (created in main)
 *   2. Populate at startup: BluetoothDeviceManager enumerates and populates
 *   3. Query devices: GetDevice(), GetDefaultOutputDevice(), GetConnectedDeviceCount()
 *   4. Update on events: AddOrUpdateDevice(), SetDefaultOutputDevice(), UpdateDeviceStage()
 *   5. Remove on disconnect: RemoveDevice()
 * 
 * KEY METHODS:
 *   - AddOrUpdateDevice(device) - Add/update a device in registry
 *   - RemoveDevice(address) - Remove device by Bluetooth address
 *   - GetDevice(address) - Retrieve specific device
 *   - GetDefaultOutputDevice() - Get primary audio device
 *   - SetDefaultOutputDevice(address) - Mark device as primary
 *   - GetConnectedDeviceCount() - Get count of connected devices
 *   - UpdateDeviceStage(address, stage) - Update device lifecycle stage
 *   - LogAllDevices() - Output all devices to debug log
 * 
 * INTEGRATION:
 *   - Used by: All modules that need device information
 *   - Populated by: BluetoothDeviceManager at startup
 *   - Updated by: HandleDeviceInterfaceArrival() on device events
 *   - Queried by: NotificationManager, DeviceContextMenu, AudioEndpointManager
 * 
 * DATA STRUCTURE:
 *   Maintains map of devices keyed by Bluetooth address (MAC)
 *   Each device tracks: name, address, connection status, battery, stage, default output flag
 */

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

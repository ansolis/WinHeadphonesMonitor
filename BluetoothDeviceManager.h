#pragma once

#include <windows.h>
#include <bluetoothapis.h>
#include <string>
#include <map>

// Struct to track a Bluetooth audio device (headphones, speakers, etc.)
struct BluetoothAudioDevice {
    std::wstring bluetoothAddress;     // MAC address (unique identifier)
    std::wstring friendlyName;         // "Sony WH-1000XM3", "AirPods Pro", etc.
    std::wstring mmdeviceId;           // Windows MMDevice GUID (when available)
    BLUETOOTH_ADDRESS btAddr;          // Parsed Bluetooth address
    bool isConnected;                  // Current Bluetooth connection state
    bool isDefaultOutput;              // Is this the primary audio output?
    BYTE batteryLevel;                 // 0-100 (if device reports it)
    ULONGLONG lastStateChangeTime;     // Timestamp of last state change
};

// Global registry of all detected Bluetooth audio devices (keyed by Bluetooth address)
extern std::map<std::wstring, BluetoothAudioDevice> g_bluetoothAudioDevices;

// Parse Bluetooth address from device ID string (12 hex chars)
// Returns true if a valid address was found and parsed
bool ParseBluetoothAddress(const std::wstring& deviceId, BLUETOOTH_ADDRESS& outAddr);

// Check if a specific Bluetooth address is marked as connected
// Enumerates paired/remembered devices and checks fConnected flag
bool IsBluetoothDeviceConnected(const BLUETOOTH_ADDRESS& btAddr);

// Scan all Bluetooth devices and check if any audio device is connected
// Populates g_bluetoothAudioDevices registry with connected audio devices
// Returns count of connected audio devices found
int EnumerateBluetoothAudioDevices();

// Check if a device name matches a known audio device pattern
// (headphones, headsets, earbuds, speakers, etc.)
bool IsAudioDeviceType(const std::wstring& deviceName);

// Scan all Bluetooth devices and check if any WH-1000XM3 is connected
// Used as the authoritative source for connection state (legacy, for backward compatibility)
bool IsAnyHeadsetConnected();

/*
 * BluetoothDeviceManager.h
 * 
 * MODULE: Bluetooth Device Manager (Generic Device Enumeration)
 * PURPOSE: Enumerate and filter Bluetooth audio devices without model-specific hardcoding
 * 
 * DESCRIPTION:
 *   Provides generic Bluetooth device enumeration that works with ANY Bluetooth audio device
 *   type (headphones, speakers, car kits, hearing aids). Does NOT hardcode device models or
 *   vendor IDs - instead filters by generic device class (wireless, audio) and connection type.
 * 
 *   Supports dynamic device discovery at application startup and during runtime monitoring.
 *   Maintains registry of detected devices with address, name, and connection status.
 * 
 * USAGE:
 *   1. Initialize Bluetooth API: BluetoothSetupServiceEx() (called in WinMain)
 *   2. Enumerate devices: EnumerateBluetoothAudioDevices() returns map of all audio devices
 *   3. Process devices: Iterate map, add each to DeviceRegistry
 *   4. Monitor changes: Via device arrival notifications in HandleDeviceInterfaceArrival()
 *   5. Parse addresses: ParseBluetoothAddress() converts ID strings to MAC addresses
 * 
 * KEY FUNCTIONS:
 *   - EnumerateBluetoothAudioDevices() - Get all Bluetooth audio devices (generic filter)
 *   - ParseBluetoothAddress() - Convert device ID string to BLUETOOTH_ADDRESS struct
 *   - IsAudioDeviceType() - Check if device class indicates audio capability
 *   - LookupDevice() - Get device from registry by address
 * 
 * DATA STRUCTURES:
 *   - BluetoothAudioDevice: Holds address, name, connection status, clock, for each device
 *   - g_bluetoothAudioDevices: Global map of all detected Bluetooth audio devices
 * 
 * GENERIC FILTERING:
 *   Does NOT check:
 *     ❌ Vendor IDs (works with Sony, Bose, Beats, JBL, Sennheiser, etc.)
 *     ❌ Model names (works with any name pattern)
 *     ❌ Specific device classes
 *   
 *   Instead checks:
 *     ✅ Device class flags (wireless, audio, rendering/capture)
 *     ✅ Bluetooth radio availability
 *     ✅ Connection state (ignored initially, detected via Stage 1/2)
 * 
 * INTEGRATION:
 *   - Called by: WinMain for startup enumeration
 *   - Uses: Windows Bluetooth APIs (bluetoothapis.h, bthdef.h)
 *   - Populates: DeviceRegistry with discovered devices
 *   - Triggers: HandleDeviceInterfaceArrival() on device changes
 * 
 * LOGGING:
 *   All output prefixed with "[BluetoothDeviceManager]" for easy filtering
 *   Examples:
 *     [BluetoothDeviceManager] Enumerating Bluetooth audio devices...
 *     [BluetoothDeviceManager] Found: 'WH-1000XM3' at CC:98:8B:56:B4:5C
 *     [BluetoothDeviceManager] Total audio devices found: 2
 */

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

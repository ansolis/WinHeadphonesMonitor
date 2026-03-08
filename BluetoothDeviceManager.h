#pragma once

#include <windows.h>
#include <bluetoothapis.h>
#include <string>

// Parse Bluetooth address from device ID string (12 hex chars)
// Returns true if a valid address was found and parsed
bool ParseBluetoothAddress(const std::wstring& deviceId, BLUETOOTH_ADDRESS& outAddr);

// Check if a specific Bluetooth address is marked as connected
// Enumerates paired/remembered devices and checks fConnected flag
bool IsBluetoothDeviceConnected(const BLUETOOTH_ADDRESS& btAddr);

// Scan all Bluetooth devices and check if any WH-1000XM3 is connected
// Used as the authoritative source for connection state
bool IsAnyHeadsetConnected();

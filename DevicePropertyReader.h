#pragma once

#include <windows.h>
#include <cfgmgr32.h>
#include <string>

// Read battery level from a devnode, walking up the parent chain
// Walks up the devnode tree once without retries to avoid noisy logs
CONFIGRET ReadBatteryFromDevInst(DEVINST devInst, BYTE& outBattery);

// Enumerate all devices and find WH-1000XM3 with battery level
// Returns a formatted string with device name and battery info
// Returns a user-friendly message if device not found
std::wstring GetHeadphonesBatteryLevel();

// Format battery information into a user-friendly string
// Used by device arrival handlers to create consistent balloon notifications
std::wstring FormatBatteryString(const std::wstring& friendlyName, BYTE batteryLevel, CONFIGRET result);

/*
 * DevicePropertyReader.h
 * 
 * MODULE: Device Property Reader (Battery & Property Extraction)
 * PURPOSE: Read device properties (battery level, status) from Bluetooth device nodes
 * 
 * DESCRIPTION:
 *   Reads device properties from Windows device tree using SetupAPI and Configuration
 *   Manager. Primary purpose is extracting battery level from Bluetooth devices.
 * 
 *   Uses DEVPROPKEY for battery percentage property and walks device node tree
 *   to find battery information. Supports multiple device hierarchy levels without
 *   noisy repeated queries.
 * 
 * USAGE:
 *   1. Get device handle from enumeration
 *   2. Call ReadBatteryFromDevInst(devInst, batteryPercent)
 *   3. Check return value (CR_SUCCESS = valid battery read)
 *   4. Use batteryPercent in notifications
 * 
 * KEY FUNCTIONS:
 *   - ReadBatteryFromDevInst() - Read battery level from device node
 *   - GetHeadphonesBatteryLevel() - Query battery for currently connected device
 *   - FormatBatteryString() - Format battery level for display (e.g., "85%")
 * 
 * BATTERY LEVEL:
 *   - Range: 0-100 (represents percentage)
 *   - Property: DEVPROPKEY with battery percentage GUID
 *   - Source: Device driver or firmware battery status
 *   - Reliability: Varies by device manufacturer implementation
 * 
 * DEVICE PROPERTY ACCESS:
 *   Uses SetupAPI and CM functions:
 *   - CM_Get_DevNode_PropertyW() - Read property value from device node
 *   - CM_Get_Parent() - Walk up parent chain for battery in parent devices
 *   - SetupDiGetDevicePropertyW() - Alternative property access method
 * 
 * PARENT CHAIN WALKING:
 *   Device nodes form a tree hierarchy. Battery may be reported at:
 *   - Device itself (leaf node)
 *   - Parent device (composite device)
 *   - Grandparent (radio/controller)
 *   
 *   Function walks up once without retry loops to minimize debug noise
 * 
 * INTEGRATION:
 *   - Called by: NotificationManager to include battery in notifications
 *   - Uses: Windows SetupAPI and Configuration Manager
 *   - Returns: CR_SUCCESS (valid read) or error code (not available)
 *   - Outputs: Battery percentage (0-100) or UNKNOWN (-1) if unavailable
 * 
 * LOGGING:
 *   All output prefixed with "[DevicePropertyReader]" for easy filtering
 *   Examples:
 *     [DevicePropertyReader] ReadBattery: 'WH-1000XM3' battery=85%
 *     [DevicePropertyReader] Battery property not found (parent not available)
 *     [DevicePropertyReader] GetHeadphonesBatteryLevel: 'Sony Device' = 92%
 * 
 * ERROR HANDLING:
 *   - CR_SUCCESS: Battery read successfully
 *   - CR_NO_SUCH_VALUE: Property doesn't exist (device doesn't report battery)
 *   - CR_NO_SUCH_DEVNODE: Device node not found
 *   - Other: SetupAPI/CM errors (logged but non-fatal)
 * 
 * FUTURE ENHANCEMENTS:
 *   - Reading other device properties (signal strength, connection time)
 *   - Multiple property queries (vendor, model, firmware version)
 *   - Property caching to reduce API calls
 *   - Support for different battery property keys by vendor
 */

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

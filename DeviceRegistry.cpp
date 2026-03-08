#include "DeviceRegistry.h"
#include <sstream>

// Global instance
DeviceRegistry* g_deviceRegistry = nullptr;

DeviceRegistry::DeviceRegistry() 
{
    OutputDebugStringW(L"[DeviceRegistry] Initialized\n");
}

DeviceRegistry::~DeviceRegistry()
{
    Clear();
}

void DeviceRegistry::AddOrUpdateDevice(const BluetoothAudioDevice& device)
{
    wchar_t msg[512];
    _snwprintf_s(msg, _countof(msg), _TRUNCATE,
        L"[DeviceRegistry] AddOrUpdateDevice: '%s' (addr=%s)\n",
        device.friendlyName.c_str(), device.bluetoothAddress.c_str());
    OutputDebugStringW(msg);

    m_devices[device.bluetoothAddress] = device;
}

void DeviceRegistry::RemoveDevice(const std::wstring& bluetoothAddress)
{
    auto it = m_devices.find(bluetoothAddress);
    if (it != m_devices.end()) {
        wchar_t msg[512];
        _snwprintf_s(msg, _countof(msg), _TRUNCATE,
            L"[DeviceRegistry] RemoveDevice: '%s' (addr=%s)\n",
            it->second.friendlyName.c_str(), bluetoothAddress.c_str());
        OutputDebugStringW(msg);
        
        m_devices.erase(it);

        // If this was the default output device, clear it
        if (m_defaultOutputAddress == bluetoothAddress) {
            m_defaultOutputAddress = L"";
        }
    }
}

BluetoothAudioDevice* DeviceRegistry::GetDevice(const std::wstring& bluetoothAddress)
{
    auto it = m_devices.find(bluetoothAddress);
    if (it != m_devices.end()) {
        return &it->second;
    }
    return nullptr;
}

const std::map<std::wstring, BluetoothAudioDevice>& DeviceRegistry::GetAllDevices() const
{
    return m_devices;
}

int DeviceRegistry::GetConnectedDeviceCount() const
{
    return static_cast<int>(m_devices.size());
}

void DeviceRegistry::UpdateDeviceStage(const std::wstring& bluetoothAddress, DeviceStage stage)
{
    auto it = m_devices.find(bluetoothAddress);
    if (it != m_devices.end()) {
        const char* stageName = "UNKNOWN";
        switch (stage) {
        case STAGE1_INTERFACES_DETECTED: stageName = "STAGE1_INTERFACES_DETECTED"; break;
        case STAGE2_ENDPOINT_READY: stageName = "STAGE2_ENDPOINT_READY"; break;
        case STAGE3_DEFAULT_OUTPUT: stageName = "STAGE3_DEFAULT_OUTPUT"; break;
        case STAGE_DISCONNECTED: stageName = "STAGE_DISCONNECTED"; break;
        }

        wchar_t msg[512];
        _snwprintf_s(msg, _countof(msg), _TRUNCATE,
            L"[DeviceRegistry] UpdateDeviceStage: '%s' -> %hs\n",
            it->second.friendlyName.c_str(), stageName);
        OutputDebugStringW(msg);
    }
}

BluetoothAudioDevice* DeviceRegistry::GetDefaultOutputDevice()
{
    if (m_defaultOutputAddress.empty()) {
        return nullptr;
    }
    return GetDevice(m_defaultOutputAddress);
}

void DeviceRegistry::SetDefaultOutputDevice(const std::wstring& bluetoothAddress)
{
    if (m_defaultOutputAddress != bluetoothAddress) {
        auto oldDevice = GetDevice(m_defaultOutputAddress);
        auto newDevice = GetDevice(bluetoothAddress);

        wchar_t msg[512];
        if (oldDevice && newDevice) {
            _snwprintf_s(msg, _countof(msg), _TRUNCATE,
                L"[DeviceRegistry] SetDefaultOutputDevice: '%s' -> '%s'\n",
                oldDevice->friendlyName.c_str(), newDevice->friendlyName.c_str());
        } else if (newDevice) {
            _snwprintf_s(msg, _countof(msg), _TRUNCATE,
                L"[DeviceRegistry] SetDefaultOutputDevice: (none) -> '%s'\n",
                newDevice->friendlyName.c_str());
        }
        OutputDebugStringW(msg);

        m_defaultOutputAddress = bluetoothAddress;

        // Update the isDefaultOutput flag for all devices
        for (auto& pair : m_devices) {
            pair.second.isDefaultOutput = (pair.first == bluetoothAddress);
        }
    }
}

void DeviceRegistry::Clear()
{
    if (!m_devices.empty()) {
        wchar_t msg[256];
        _snwprintf_s(msg, _countof(msg), _TRUNCATE,
            L"[DeviceRegistry] Clear: removing %zu devices\n", m_devices.size());
        OutputDebugStringW(msg);
    }
    m_devices.clear();
    m_defaultOutputAddress = L"";
}

void DeviceRegistry::LogAllDevices() const
{
    if (m_devices.empty()) {
        OutputDebugStringW(L"[DeviceRegistry] No devices registered\n");
        return;
    }

    wchar_t header[256];
    _snwprintf_s(header, _countof(header), _TRUNCATE,
        L"[DeviceRegistry] Registered devices (%zu):\n", m_devices.size());
    OutputDebugStringW(header);

    for (const auto& pair : m_devices) {
        const auto& device = pair.second;
        wchar_t line[512];
        _snwprintf_s(line, _countof(line), _TRUNCATE,
            L"  - '%s' (addr=%s) connected=%d default=%d battery=%d%%\n",
            device.friendlyName.c_str(), device.bluetoothAddress.c_str(),
            device.isConnected, device.isDefaultOutput, device.batteryLevel);
        OutputDebugStringW(line);
    }
}

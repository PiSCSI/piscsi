//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2021-2023 Uwe Seimet
//
// The DeviceFactory creates devices based on their type and the image file extension
//
//---------------------------------------------------------------------------

#pragma once

#include "devices/device.h"
#include <string>
#include <unordered_set>
#include <unordered_map>
#include "generated/piscsi_interface.pb.h"

using namespace std;
using namespace piscsi_interface;

class PrimaryDevice;

class DeviceFactory
{
	const inline static string DEFAULT_IP = "10.10.20.1/24"; //NOSONAR This hardcoded IP address is safe

public:

	DeviceFactory();
	~DeviceFactory() = default;

	shared_ptr<PrimaryDevice> CreateDevice(PbDeviceType, int, const string&) const;
	PbDeviceType GetTypeForFile(const string&) const;
	const unordered_set<uint32_t>& GetSectorSizes(PbDeviceType type) const;
	const param_map& GetDefaultParams(PbDeviceType type) const;
	const auto& GetExtensionMapping() const { return extension_mapping; }

private:

	unordered_map<PbDeviceType, unordered_set<uint32_t>> sector_sizes;

	unordered_map<PbDeviceType, param_map> default_params;

	unordered_map<string, PbDeviceType, hash<string>> extension_mapping;

	unordered_map<string, PbDeviceType, hash<string>> device_mapping;

	inline static const unordered_set<uint32_t> EMPTY_SET;
	inline static const param_map EMPTY_PARAM_MAP;
};

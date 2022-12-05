//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
// The DeviceFactory creates devices based on their type and the image file extension
//
//---------------------------------------------------------------------------

#pragma once

#include <string>
#include <vector>
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
	const unordered_map<string, string>& GetDefaultParams(PbDeviceType type) const;
	vector<string> GetNetworkInterfaces() const;
	const unordered_map<string, PbDeviceType>& GetExtensionMapping() const { return extension_mapping; }

private:

	unordered_map<PbDeviceType, unordered_set<uint32_t>> sector_sizes;

	unordered_map<PbDeviceType, unordered_map<string, string>> default_params;

	unordered_map<string, PbDeviceType> extension_mapping;

	unordered_map<string, PbDeviceType> device_mapping;

	unordered_set<uint32_t> empty_set;
	unordered_map<string, string> empty_map;
};

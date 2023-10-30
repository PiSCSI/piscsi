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

#include "shared/piscsi_util.h"
#include <string>
#include <unordered_set>
#include <unordered_map>
#include "generated/piscsi_interface.pb.h"

using namespace std;
using namespace piscsi_interface;

class PrimaryDevice;

class DeviceFactory
{

public:

	DeviceFactory();
	~DeviceFactory() = default;

	shared_ptr<PrimaryDevice> CreateDevice(PbDeviceType, int, const string&) const;
	PbDeviceType GetTypeForFile(const string&) const;
	unordered_set<uint32_t> GetSectorSizes(PbDeviceType type) const;
	const auto& GetExtensionMapping() const { return extension_mapping; }

private:

	unordered_map<PbDeviceType, unordered_set<uint32_t>> sector_sizes;

	unordered_map<string, PbDeviceType, piscsi_util::StringHash, equal_to<>> extension_mapping;

	unordered_map<string, PbDeviceType, piscsi_util::StringHash, equal_to<>> device_mapping;
};

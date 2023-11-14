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

#include <string>
#include <unordered_map>
#include "generated/piscsi_interface.pb.h"

using namespace std;
using namespace piscsi_interface;

class PrimaryDevice;

class DeviceFactory
{

public:

	DeviceFactory() = default;
	~DeviceFactory() = default;

	shared_ptr<PrimaryDevice> CreateDevice(PbDeviceType, int, const string&) const;
	PbDeviceType GetTypeForFile(const string&) const;
	const auto& GetExtensionMapping() const { return EXTENSION_MAPPING; }

private:

	const inline static unordered_map<string, PbDeviceType, piscsi_util::StringHash, equal_to<>> EXTENSION_MAPPING = {
			{ "hd1", SCHD },
			{ "hds", SCHD },
			{ "hda", SCHD },
			{ "hdn", SCHD },
			{ "hdi", SCHD },
			{ "nhd", SCHD },
			{ "hdr", SCRM },
			{ "mos", SCMO },
			{ "is1", SCCD },
			{ "iso", SCCD }
	};

	const inline static unordered_map<string, PbDeviceType, piscsi_util::StringHash, equal_to<>> DEVICE_MAPPING = {
			{ "bridge", SCBR },
			{ "daynaport", SCDP },
			{ "printer", SCLP },
			{ "services", SCHS }
	};
};

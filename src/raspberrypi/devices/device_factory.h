//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
// The DeviceFactory singleton creates devices based on their type and the extension of their image file
//
//---------------------------------------------------------------------------

#pragma once

#include <set>
#include <string>
#include "rascsi_interface.pb.h"

class Device;

class DeviceFactory
{
public:

	DeviceFactory();
	~DeviceFactory();

	static DeviceFactory& instance();

	const std::set<int>& GetSasiSectorSizes() const { return sector_sizes_sasi; };
	const std::set<int>& GetScsiSectorSizes() const { return sector_sizes_scsi; };

	Device *CreateDevice(rascsi_interface::PbDeviceType& type, const std::string& filename, const std::string& ext);

private:

	std::set<int> sector_sizes_sasi;
	std::set<int> sector_sizes_scsi;
};

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
#include <map>
#include <string>
#include "rascsi_interface.pb.h"

using namespace std;

typedef pair<uint32_t, uint32_t> Geometry;

class Device;

class DeviceFactory
{
public:

	DeviceFactory();
	~DeviceFactory() {};

	static DeviceFactory& instance();

	const set<uint32_t>& GetSasiSectorSizes() const { return sector_sizes_sasi; };
	const set<uint32_t>& GetScsiSectorSizes() const { return sector_sizes_scsi; };

	const set<uint64_t> GetMoCapacities() const;

	Device *CreateDevice(rascsi_interface::PbDeviceType& type, const string& filename, const string& ext);

private:

	set<uint32_t> sector_sizes_sasi;
	set<uint32_t> sector_sizes_scsi;

	// Mapping of supported MO capacities in bytes to the respective block sizes and block counts
	map<uint64_t, Geometry> geometries_mo;
};

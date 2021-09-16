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
#include <list>
#include <map>
#include <string>
#include "rascsi_interface.pb.h"

using namespace std;
using namespace rascsi_interface;

typedef pair<uint32_t, uint32_t> Geometry;

class Device;

class DeviceFactory
{
public:

	DeviceFactory();
	~DeviceFactory() {};

	static DeviceFactory& instance();

	const set<uint32_t>& GetSectorSizes(PbDeviceType type) { return sector_sizes[type]; }
	const set<uint32_t>& GetSectorSizes(const string&);
	const set<uint64_t> GetCapacities(PbDeviceType);
	const map<string, string>& GetDefaultParams(PbDeviceType type) { return default_params[type]; }

	Device *CreateDevice(PbDeviceType type, const string& filename, const string& ext);

private:

	map<PbDeviceType, set<uint32_t>> sector_sizes;

	map<PbDeviceType, map<uint64_t, Geometry>> geometries;

	map<PbDeviceType, map<string, string>> default_params;
};

//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
// The DeviceFactory singleton creates devices based on their type and the image file extension
//
//---------------------------------------------------------------------------

#pragma once

#include <list>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <string>
#include "rascsi_interface.pb.h"

using namespace std;
using namespace rascsi_interface;

using Geometry = pair<uint32_t, uint32_t>;

class PrimaryDevice;

class DeviceFactory
{
	DeviceFactory();
	~DeviceFactory() = default;
	DeviceFactory(DeviceFactory&) = delete;
	DeviceFactory& operator=(const DeviceFactory&) = delete;

public:

	static DeviceFactory& instance();

	PrimaryDevice *CreateDevice(PbDeviceType, const string&, int);
	void DeleteDevice(const PrimaryDevice&) const;
	void DeleteAllDevices() const;
	const PrimaryDevice *GetDeviceByIdAndLun(int, int) const;
	list<PrimaryDevice *> GetAllDevices() const;
	PbDeviceType GetTypeForFile(const string&) const;
	const unordered_set<uint32_t>& GetSectorSizes(PbDeviceType type) { return sector_sizes[type]; }
	const unordered_set<uint32_t>& GetSectorSizes(const string&) const;
	const unordered_map<string, string>& GetDefaultParams(PbDeviceType type) { return default_params[type]; }
	list<string> GetNetworkInterfaces() const;
	unordered_map<string, PbDeviceType> GetExtensionMapping() const { return extension_mapping; }

private:

	unordered_map<PbDeviceType, unordered_set<uint32_t>> sector_sizes;

	// Optional mapping of drive capacities to drive geometries
	unordered_map<PbDeviceType, unordered_map<uint64_t, Geometry>> geometries;

	unordered_map<PbDeviceType, unordered_map<string, string>> default_params;

	unordered_map<string, PbDeviceType> extension_mapping;

	string GetExtension(const string&) const;

	static std::multimap<int, unique_ptr<PrimaryDevice>> devices;
};

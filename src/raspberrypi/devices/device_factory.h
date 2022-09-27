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

using namespace std; //NOSONAR Not relevant for rascsi
using namespace rascsi_interface; //NOSONAR Not relevant for rascsi

class PrimaryDevice;

class DeviceFactory
{
public:

	DeviceFactory();
	~DeviceFactory() = default;

	PrimaryDevice *CreateDevice(PbDeviceType, const string&, int);
	void DeleteDevice(const PrimaryDevice&) const;
	void DeleteAllDevices() const;
	const PrimaryDevice *GetDeviceByIdAndLun(int, int) const;
	list<PrimaryDevice *> GetAllDevices() const;
	PbDeviceType GetTypeForFile(const string&) const;
	const unordered_set<uint32_t>& GetSectorSizes(PbDeviceType type) const;
	const unordered_set<uint32_t>& GetSectorSizes(const string&) const;
	const unordered_map<string, string>& GetDefaultParams(PbDeviceType type) const;
	list<string> GetNetworkInterfaces() const;
	unordered_map<string, PbDeviceType> GetExtensionMapping() const { return extension_mapping; }

private:

	unordered_map<PbDeviceType, unordered_set<uint32_t>> sector_sizes;

	unordered_map<PbDeviceType, unordered_map<string, string>> default_params;

	unordered_map<string, PbDeviceType> extension_mapping;

	string GetExtension(const string&) const;

	static std::multimap<int, unique_ptr<PrimaryDevice>> devices;

	unordered_set<uint32_t> empty_set;
	unordered_map<string, string> empty_map;
};

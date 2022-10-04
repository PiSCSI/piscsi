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

#include <string>
#include <list>
#include <unordered_set>
#include <unordered_map>
#include "rascsi_interface.pb.h"

using namespace std;
using namespace rascsi_interface;

class ControllerManager;
class PrimaryDevice;

class DeviceFactory
{
	const string DEFAULT_IP = "10.10.20.1/24";

public:

	DeviceFactory();
	~DeviceFactory() = default;

	shared_ptr<PrimaryDevice> CreateDevice(const ControllerManager&, PbDeviceType, int, const string&);
	PbDeviceType GetTypeForFile(const string&) const;
	const unordered_set<uint32_t>& GetSectorSizes(PbDeviceType type) const;
	const unordered_set<uint32_t>& GetSectorSizes(const string&) const;
	const unordered_map<string, string>& GetDefaultParams(PbDeviceType type) const;
	list<string> GetNetworkInterfaces() const;
	unordered_map<string, PbDeviceType> GetExtensionMapping() const { return extension_mapping; }

private:

	string GetExtension(const string&) const;

	unordered_map<PbDeviceType, unordered_set<uint32_t>> sector_sizes;

	unordered_map<PbDeviceType, unordered_map<string, string>> default_params;

	unordered_map<string, PbDeviceType> extension_mapping;

	unordered_set<uint32_t> empty_set;
	unordered_map<string, string> empty_map;
};

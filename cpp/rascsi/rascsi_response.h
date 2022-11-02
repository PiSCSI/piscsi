//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "rascsi_interface.pb.h"
#include <dirent.h>
#include <array>
#include <string>

using namespace std;
using namespace rascsi_interface;

class DeviceFactory;
class ControllerManager;
class Device;

class RascsiResponse
{
public:

	explicit RascsiResponse(int max_luns) : max_luns(max_luns) {}
	~RascsiResponse() = default;

	bool GetImageFile(PbImageFile&, const string&, const string&) const;
	unique_ptr<PbImageFilesInfo> GetAvailableImages(PbResult&, const string&, const string&, const string&, int) const;
	unique_ptr<PbReservedIdsInfo> GetReservedIds(PbResult&, const unordered_set<int>&) const;
	void GetDevices(const ControllerManager&, PbServerInfo&, const string&) const;
	void GetDevicesInfo(const ControllerManager&, PbResult&, const PbCommand&, const string&) const;
	unique_ptr<PbDeviceTypesInfo> GetDeviceTypesInfo(PbResult&) const;
	unique_ptr<PbVersionInfo> GetVersionInfo(PbResult&) const;
	unique_ptr<PbServerInfo> GetServerInfo(const ControllerManager&, PbResult&, const unordered_set<int>&,
			const string&, const string&, const string&, const string&, int) const;
	unique_ptr<PbNetworkInterfacesInfo> GetNetworkInterfacesInfo(PbResult&) const;
	unique_ptr<PbMappingInfo> GetMappingInfo(PbResult&) const;
	unique_ptr<PbLogLevelInfo> GetLogLevelInfo(PbResult&, const string&) const;
	unique_ptr<PbOperationInfo> GetOperationInfo(PbResult&, int) const;

private:

	DeviceFactory device_factory;

	int max_luns;

	const array<string, 6> log_levels = { "trace", "debug", "info", "warn", "err", "off" };

	unique_ptr<PbDeviceProperties> GetDeviceProperties(const Device&) const;
	void GetDevice(const Device&, PbDevice&, const string&) const;
	void GetAllDeviceTypeProperties(PbDeviceTypesInfo&) const;
	void GetDeviceTypeProperties(PbDeviceTypesInfo&, PbDeviceType) const;
	void GetAvailableImages(PbImageFilesInfo&, const string&, const string&, const string&, const string&, int) const;
	void GetAvailableImages(PbResult& result, PbServerInfo&, const string&, const string&, const string&, int) const;
	unique_ptr<PbOperationMetaData> CreateOperation(PbOperationInfo&, const PbOperation&, const string&) const;
	unique_ptr<PbOperationParameter> AddOperationParameter(PbOperationMetaData&, const string&, const string&,
			const string& = "", bool = false) const;
	set<id_set> MatchDevices(const ControllerManager&, PbResult&, const PbCommand&) const;

	static string GetNextImageFile(const dirent *, const string&);
};

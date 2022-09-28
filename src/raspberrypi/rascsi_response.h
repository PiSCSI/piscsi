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
#include <list>
#include <string>

using namespace rascsi_interface; //NOSONAR Not relevant for rascsi

class DeviceFactory;
class Device;

class RascsiResponse
{
public:

	RascsiResponse(DeviceFactory& device_factory, int max_luns) : device_factory(device_factory), max_luns(max_luns) {}
	~RascsiResponse() = default;

	bool GetImageFile(PbImageFile&, const string&, const string&) const;
	PbImageFilesInfo *GetAvailableImages(PbResult&, const string&, const string&, const string&, int);
	PbReservedIdsInfo *GetReservedIds(PbResult&, const unordered_set<int>&);
	void GetDevices(PbServerInfo&, const string&);
	void GetDevicesInfo(PbResult&, const PbCommand&, const string&);
	PbDeviceTypesInfo *GetDeviceTypesInfo(PbResult&);
	PbVersionInfo *GetVersionInfo(PbResult&);
	PbServerInfo *GetServerInfo(PbResult&, const unordered_set<int>&, const string&, const string&, const string&,
			const string&, int);
	PbNetworkInterfacesInfo *GetNetworkInterfacesInfo(PbResult&);
	PbMappingInfo *GetMappingInfo(PbResult&);
	PbLogLevelInfo *GetLogLevelInfo(PbResult&, const string&);
	PbOperationInfo *GetOperationInfo(PbResult&, int);

private:

	DeviceFactory& device_factory;

	int max_luns;

	const list<string> log_levels = { "trace", "debug", "info", "warn", "err", "critical", "off" };

	PbDeviceProperties *GetDeviceProperties(const Device&);
	void GetDevice(const Device&, PbDevice&, const string&);
	void GetAllDeviceTypeProperties(PbDeviceTypesInfo&);
	void GetDeviceTypeProperties(PbDeviceTypesInfo&, PbDeviceType);
	void GetAvailableImages(PbImageFilesInfo&, const string&, const string&, const string&, const string&, int);
	void GetAvailableImages(PbResult& result, PbServerInfo&, const string&, const string&, const string&, int);
	PbOperationMetaData *CreateOperation(PbOperationInfo&, const PbOperation&, const string&) const;
	PbOperationParameter *AddOperationParameter(PbOperationMetaData&, const string&, const string&,
			const string& = "", bool = false);

	static string GetNextImageFile(const dirent *, const string&);
};

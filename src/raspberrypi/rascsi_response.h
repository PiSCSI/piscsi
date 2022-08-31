//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "devices/device_factory.h"
#include "rascsi_interface.pb.h"
#include <list>
#include <string>

using namespace std;
using namespace rascsi_interface;

class DeviceFactory;
class RascsiImage;
class Device;

class RascsiResponse
{
public:

	RascsiResponse(DeviceFactory *device_factory, const RascsiImage *rascsi_image)
		: device_factory(device_factory), rascsi_image(rascsi_image) {}
	~RascsiResponse() {}

	bool GetImageFile(PbImageFile *, const string&);
	PbImageFilesInfo *GetAvailableImages(PbResult&, const string&, const string&, int);
	PbReservedIdsInfo *GetReservedIds(PbResult&, const unordered_set<int>&);
	void GetDevices(PbServerInfo&);
	void GetDevicesInfo(PbResult&, const PbCommand&);
	PbDeviceTypesInfo *GetDeviceTypesInfo(PbResult&);
	PbVersionInfo *GetVersionInfo(PbResult&);
	PbServerInfo *GetServerInfo(PbResult&, const unordered_set<int>&, const string&, const string&, const string&, int);
	PbNetworkInterfacesInfo *GetNetworkInterfacesInfo(PbResult&);
	PbMappingInfo *GetMappingInfo(PbResult&);
	PbLogLevelInfo *GetLogLevelInfo(PbResult&, const string&);
	PbOperationInfo *GetOperationInfo(PbResult&, int);

private:

	DeviceFactory *device_factory;
	const RascsiImage *rascsi_image;

	static list<string> log_levels;

	PbDeviceProperties *GetDeviceProperties(const Device *);
	void GetDevice(const Device *, PbDevice *);
	void GetAllDeviceTypeProperties(PbDeviceTypesInfo&);
	void GetDeviceTypeProperties(PbDeviceTypesInfo&, PbDeviceType);
	void GetAvailableImages(PbImageFilesInfo&, const string&, const string&, const string&, const string&, int);
	void GetAvailableImages(PbResult& result, PbServerInfo&, const string&, const string&, int);
	void CreateOperation(PbOperationInfo *, PbOperationMetaData *, const PbOperation&, const string&);
	PbOperationParameter *AddOperationParameter(PbOperationMetaData *, const string&, const string&,
			const string& = "", bool = false);
};

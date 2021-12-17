//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "devices/device_factory.h"
#include "rascsi_interface.pb.h"
#include <vector>
#include <string>

using namespace std;
using namespace rascsi_interface;

class DeviceFactory;
class RascsiImage;
class Device;

class RascsiResponse
{
public:

	RascsiResponse(DeviceFactory *, const RascsiImage *);
	~RascsiResponse() {};

	bool GetImageFile(PbImageFile *, const string&);
	PbImageFilesInfo *GetAvailableImages(PbResult&);
	void GetDevices(PbServerInfo&, const vector<Device *>&);
	void GetDevicesInfo(PbResult&, const PbCommand&, const vector<Device *>&, int);
	PbDeviceTypesInfo *GetDeviceTypesInfo(PbResult&, const PbCommand&);
	PbVersionInfo *GetVersionInfo(PbResult&);
	PbServerInfo *GetServerInfo(PbResult&, const vector<Device *>&, const set<int>&, const string&);
	PbNetworkInterfacesInfo *GetNetworkInterfacesInfo(PbResult&);
	PbLogLevelInfo *GetLogLevelInfo(PbResult&, const string&);
	PbReservedIdsInfo *GetReservedIds(PbResult&, const set<int>&);
	PbMappingInfo *GetMappingInfo(PbResult&);
	PbOperationInfo *GetOperationInfo(PbResult&);

private:

	DeviceFactory *device_factory;
	const RascsiImage *rascsi_image;

	vector<string> log_levels { "trace", "debug", "info", "warn", "err", "critical", "off" };

	PbDeviceProperties *GetDeviceProperties(const Device *);
	void GetDevice(const Device *, PbDevice *);
	void GetAllDeviceTypeProperties(PbDeviceTypesInfo&);
	void GetDeviceTypeProperties(PbDeviceTypesInfo&, PbDeviceType);
	void GetAvailableImages(PbResult& result, PbServerInfo&);
};

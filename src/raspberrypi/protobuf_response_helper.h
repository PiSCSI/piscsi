//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
// A singleton that creates responses for protobuf interface requests
//
//---------------------------------------------------------------------------

#pragma once

#include "devices/device_factory.h"
#include "rascsi_interface.pb.h"
#include <vector>
#include <string>

using namespace std;
using namespace rascsi_interface;

class Device;

class ProtobufResponseHandler
{
public:

	ProtobufResponseHandler();
	~ProtobufResponseHandler() {};

	static ProtobufResponseHandler& instance();

	PbDeviceProperties *GetDeviceProperties(const Device *);
	void GetDeviceTypeProperties(PbDeviceTypesInfo&, PbDeviceType);
	void GetAllDeviceTypeProperties(PbDeviceTypesInfo&);
	void GetDevice(const Device *, PbDevice *, const string&);
	void GetImageFile(PbImageFile *, const string&, const string&);
	void GetAvailableImages(PbImageFilesInfo&, const string&);
	void GetAvailableImages(PbServerInfo&, const string&);
	void GetDevices(PbServerInfo&, const vector<Device *>&, const string&);
	void GetDevicesInfo(PbResult&, const PbCommand&, const vector<Device *>&, const string&, int);
	void GetDeviceTypesInfo(PbResult&, const PbCommand&);
	void GetServerInfo(PbResult&, const vector<Device *>&, const set<int>&, const string&, const string&);
	void GetLogLevels(PbServerInfo&);
	void GetNetworkInterfacesInfo(PbNetworkInterfacesInfo&);

private:

	DeviceFactory device_factory;

	vector<string> log_levels;
};

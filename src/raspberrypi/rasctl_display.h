//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "rascsi_interface.pb.h"

using namespace rascsi_interface;

class RasctlDisplay
{
public:

	RasctlDisplay() {};
	~RasctlDisplay() {};

	void DisplayDevices(const PbDevicesInfo&);
	void DisplayDeviceInfo(const PbDevice&);
	void DisplayVersionInfo(const PbVersionInfo&);
	void DisplayLogLevelInfo(const PbLogLevelInfo&);
	void DisplayDeviceTypesInfo(const PbDeviceTypesInfo&);
	void DisplayReservedIdsInfo(const PbReservedIdsInfo&);
	void DisplayImageFile(const PbImageFile&);
	void DisplayImageFiles(const PbImageFilesInfo&);
	void DisplayNetworkInterfaces(const PbNetworkInterfacesInfo&);
	void DisplayMappingInfo(const PbMappingInfo&);
};

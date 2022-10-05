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

using namespace rascsi_interface;

class RasctlDisplay
{
public:

	RasctlDisplay() = default;
	~RasctlDisplay() = default;

	void DisplayDevices(const PbDevicesInfo&) const;
	void DisplayDeviceInfo(const PbDevice&) const;
	void DisplayVersionInfo(const PbVersionInfo&) const;
	void DisplayLogLevelInfo(const PbLogLevelInfo&) const;
	void DisplayDeviceTypesInfo(const PbDeviceTypesInfo&) const;
	void DisplayReservedIdsInfo(const PbReservedIdsInfo&) const;
	void DisplayImageFile(const PbImageFile&) const;
	void DisplayImageFiles(const PbImageFilesInfo&) const;
	void DisplayNetworkInterfaces(const PbNetworkInterfacesInfo&) const;
	void DisplayMappingInfo(const PbMappingInfo&) const;
	void DisplayOperationInfo(const PbOperationInfo&) const;

private:

	void DisplayParams(const PbDevice&) const;
	void DisplayBlockSizes(const PbDeviceProperties&) const;
};

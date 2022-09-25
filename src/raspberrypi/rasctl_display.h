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

using namespace rascsi_interface; //NOSONAR Not relevant for rascsi

class RasctlDisplay
{
	friend class RasctlCommands;

	RasctlDisplay() = default;
	~RasctlDisplay() = default;
	RasctlDisplay(RasctlDisplay&) = delete;
	RasctlDisplay& operator=(const RasctlDisplay&) = delete;

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
};

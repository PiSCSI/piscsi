//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2021-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "generated/piscsi_interface.pb.h"
#include <string>

using namespace std;
using namespace piscsi_interface;

class ScsictlDisplay
{
public:

	ScsictlDisplay() = default;
	~ScsictlDisplay() = default;

	string DisplayDevicesInfo(const PbDevicesInfo&) const;
	string DisplayDeviceInfo(const PbDevice&) const;
	string DisplayVersionInfo(const PbVersionInfo&) const;
	string DisplayLogLevelInfo(const PbLogLevelInfo&) const;
	string DisplayDeviceTypesInfo(const PbDeviceTypesInfo&) const;
	string DisplayReservedIdsInfo(const PbReservedIdsInfo&) const;
	string DisplayImageFile(const PbImageFile&) const;
	string DisplayImageFilesInfo(const PbImageFilesInfo&) const;
	string DisplayNetworkInterfaces(const PbNetworkInterfacesInfo&) const;
	string DisplayMappingInfo(const PbMappingInfo&) const;
	string DisplayOperationInfo(const PbOperationInfo&) const;

private:

	string DisplayParams(const PbDevice&) const;
	string DisplayAttributes(const PbDeviceProperties&) const;
	string DisplayDefaultParameters(const PbDeviceProperties&) const;
	string DisplayBlockSizes(const PbDeviceProperties&) const;
	string DisplayParameters(const PbOperationMetaData&) const;
	string DisplayPermittedValues(const PbOperationParameter&) const;
};

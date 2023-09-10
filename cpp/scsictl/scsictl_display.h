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
#include <sstream>

using namespace std;
using namespace piscsi_interface;

class ScsictlDisplay
{
	string JoinWithComma(const auto& collection) const {
		ostringstream s;

		bool isFirst = true;
		for (const auto& element : collection) {
			if (!isFirst) {
				s << ", ";
			}

			isFirst = false;

			s << element;
		}

		return s.str();
	}

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

	void DisplayParams(ostringstream&, const PbDevice&) const;
	void DisplayAttributes(ostringstream&, const PbDeviceProperties&) const;
	void DisplayDefaultParameters(ostringstream&, const PbDeviceProperties&) const;
	void DisplayBlockSizes(ostringstream&, const PbDeviceProperties&) const;
	void DisplayParameters(ostringstream&, const PbOperationMetaData&) const;
	void DisplayPermittedValues(ostringstream&, const PbOperationParameter&) const;
};

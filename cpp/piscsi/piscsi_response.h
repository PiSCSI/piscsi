//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2021-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "devices/device_factory.h"
#include "devices/primary_device.h"
#include "generated/piscsi_interface.pb.h"
#include <string>

using namespace std;
using namespace filesystem;
using namespace piscsi_interface;

class PiscsiResponse
{
	using id_set = pair<int, int>;

public:

	PiscsiResponse() = default;
	~PiscsiResponse() = default;

	bool GetImageFile(PbImageFile&, const string&, const string&) const;
	unique_ptr<PbImageFilesInfo> GetAvailableImages(PbResult&, const string&, const string&, const string&, int) const;
	unique_ptr<PbReservedIdsInfo> GetReservedIds(PbResult&, const unordered_set<int>&) const;
	void GetDevices(const unordered_set<shared_ptr<PrimaryDevice>>&, PbServerInfo&, const string&) const;
	void GetDevicesInfo(const unordered_set<shared_ptr<PrimaryDevice>>&, PbResult&, const PbCommand&, const string&) const;
	unique_ptr<PbDeviceTypesInfo> GetDeviceTypesInfo(PbResult&) const;
	unique_ptr<PbVersionInfo> GetVersionInfo(PbResult&) const;
	unique_ptr<PbServerInfo> GetServerInfo(const unordered_set<shared_ptr<PrimaryDevice>>&, PbResult&, const unordered_set<int>&,
			const string&, const string&, const string&, int) const;
	unique_ptr<PbNetworkInterfacesInfo> GetNetworkInterfacesInfo(PbResult&) const;
	unique_ptr<PbMappingInfo> GetMappingInfo(PbResult&) const;
	unique_ptr<PbLogLevelInfo> GetLogLevelInfo(PbResult&) const;
	unique_ptr<PbOperationInfo> GetOperationInfo(PbResult&, int) const;

private:

	const DeviceFactory device_factory;

	unique_ptr<PbDeviceProperties> GetDeviceProperties(const Device&) const;
	void GetDevice(const Device&, PbDevice&, const string&) const;
	void GetAllDeviceTypeProperties(PbDeviceTypesInfo&) const;
	void GetDeviceTypeProperties(PbDeviceTypesInfo&, PbDeviceType) const;
	void GetAvailableImages(PbImageFilesInfo&, const string&, const string&, const string&, int) const;
	void GetAvailableImages(PbResult& result, PbServerInfo&, const string&, const string&, const string&, int) const;
	PbOperationMetaData *CreateOperation(PbOperationInfo&, const PbOperation&, const string&) const;
	PbOperationParameter *AddOperationParameter(PbOperationMetaData&, const string&, const string&,
			const string& = "", bool = false) const;
	set<id_set> MatchDevices(const unordered_set<shared_ptr<PrimaryDevice>>&, PbResult&, const PbCommand&) const;

	static bool ValidateImageFile(const path&);

	static bool FilterMatches(const string&, string_view);
};

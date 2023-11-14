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
#include "shared/piscsi_util.h"
#include "generated/piscsi_interface.pb.h"
#include <string>
#include <span>
#include <set>

using namespace std;
using namespace filesystem;
using namespace piscsi_interface;

class PiscsiResponse
{
public:

	PiscsiResponse() = default;
	~PiscsiResponse() = default;

	bool GetImageFile(PbImageFile&, const string&, const string&) const;
	void GetImageFilesInfo(PbImageFilesInfo&, const string&, const string&, const string&, int) const;
	void GetReservedIds(PbReservedIdsInfo&, const unordered_set<int>&) const;
	void GetDevices(const unordered_set<shared_ptr<PrimaryDevice>>&, PbServerInfo&, const string&) const;
	void GetDevicesInfo(const unordered_set<shared_ptr<PrimaryDevice>>&, PbResult&, const PbCommand&, const string&) const;
	void GetDeviceTypesInfo(PbDeviceTypesInfo&) const;
	void GetVersionInfo(PbVersionInfo&) const;
	void GetServerInfo(PbServerInfo&, const PbCommand&, const unordered_set<shared_ptr<PrimaryDevice>>&,
			const unordered_set<int>&, const string&, int) const;
	void GetNetworkInterfacesInfo(PbNetworkInterfacesInfo&) const;
	void GetMappingInfo(PbMappingInfo&) const;
	void GetLogLevelInfo(PbLogLevelInfo&) const;
	void GetStatisticsInfo(PbStatisticsInfo&, const unordered_set<shared_ptr<PrimaryDevice>>&) const;
	void GetOperationInfo(PbOperationInfo&, int) const;

private:

	inline static const vector<string> EMPTY_VECTOR;

	[[no_unique_address]] const DeviceFactory device_factory;

	void GetDeviceProperties(shared_ptr<Device>, PbDeviceProperties&) const;
	void GetDevice(shared_ptr<Device>, PbDevice&, const string&) const;
	void GetDeviceTypeProperties(PbDeviceTypesInfo&, PbDeviceType) const;
	void GetAvailableImages(PbImageFilesInfo&, const string&, const string&, const string&, int) const;
	void GetAvailableImages(PbServerInfo&, const string&, const string&, const string&, int) const;
	PbOperationMetaData *CreateOperation(PbOperationInfo&, const PbOperation&, const string&) const;
	void AddOperationParameter(PbOperationMetaData&, const string&, const string&,
			const string& = "", bool = false, const vector<string>& = EMPTY_VECTOR) const;
	set<id_set> MatchDevices(const unordered_set<shared_ptr<PrimaryDevice>>&, PbResult&, const PbCommand&) const;

	static bool ValidateImageFile(const path&);

	static bool FilterMatches(const string&, string_view);

	static bool HasOperation(const set<string, less<>>&, PbOperation);
};

//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/piscsi_version.h"
#include "controllers/controller_manager.h"
#include "devices/device_factory.h"
#include "generated/piscsi_interface.pb.h"
#include "piscsi/piscsi_response.h"

using namespace piscsi_interface;

TEST(PiscsiResponseTest, Operation_Count)
{
	PiscsiResponse response;
	PbResult result;

	const auto info = response.GetOperationInfo(result, 0);
	EXPECT_EQ(PbOperation_ARRAYSIZE - 1, info->operations_size());
}

void TestNonDiskDevice(PbDeviceType type, int default_param_count)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	DeviceFactory device_factory;
	PiscsiResponse response;

	auto d = device_factory.CreateDevice(type, 0, "");
	const unordered_map<string, string> params;
	d->Init(params);
	EXPECT_TRUE(controller_manager->AttachToScsiController(0, d));

	PbServerInfo info;
	response.GetDevices(controller_manager->GetAllDevices(), info, "image_folder");

	EXPECT_EQ(1, info.devices_info().devices().size());

	const auto& device = info.devices_info().devices()[0];
	EXPECT_FALSE(device.properties().read_only());
	EXPECT_FALSE(device.properties().protectable());
	EXPECT_FALSE(device.properties().stoppable());
	EXPECT_FALSE(device.properties().removable());
	EXPECT_FALSE(device.properties().lockable());
	EXPECT_EQ(32, device.properties().luns());
	EXPECT_EQ(0, device.block_size());
	EXPECT_EQ(0, device.block_count());
	EXPECT_EQ(default_param_count, device.properties().default_params().size());
	EXPECT_EQ(default_param_count, device.params().size());
	EXPECT_FALSE(device.properties().supports_file());
	if (default_param_count) {
		EXPECT_TRUE(device.properties().supports_params());
	}
	else {
		EXPECT_FALSE(device.properties().supports_params());
	}
}

TEST(PiscsiResponseTest, GetDevices)
{
	TestNonDiskDevice(SCHS, 0);
	TestNonDiskDevice(SCLP, 1);
}

TEST(PiscsiResponseTest, GetImageFile)
{
	PiscsiResponse response;
	PbImageFile image_file;

	EXPECT_FALSE(response.GetImageFile(image_file, "default_folder", ""));

	// Even though the call fails (non-existing file) some properties must be set
	EXPECT_FALSE(response.GetImageFile(image_file, "default_folder", "filename.hds"));
	EXPECT_EQ("filename.hds", image_file.name());
	EXPECT_EQ(SCHD, image_file.type());
}

TEST(PiscsiResponseTest, GetReservedIds)
{
	PiscsiResponse response;
	unordered_set<int> ids;
	PbResult result;

	const auto& info1 = response.GetReservedIds(result, ids);
	EXPECT_TRUE(result.status());
	EXPECT_TRUE(info1->ids().empty());

	ids.insert(3);
	const auto& info2 = response.GetReservedIds(result, ids);
	EXPECT_TRUE(result.status());
	EXPECT_EQ(1, info2->ids().size());
	EXPECT_EQ(3, info2->ids()[0]);
}

TEST(PiscsiResponseTest, GetDevicesInfo)
{
	const int ID = 2;
	const int LUN1 = 0;
	const int LUN2 = 5;
	const int LUN3 = 6;

	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	PiscsiResponse response;
	PbCommand command;
	PbResult result;

	response.GetDevicesInfo(controller_manager->GetAllDevices(), result, command, "");
	EXPECT_TRUE(result.status());
	EXPECT_TRUE(result.devices_info().devices().empty());

	auto device1 = make_shared<MockHostServices>(LUN1);
	EXPECT_TRUE(controller_manager->AttachToScsiController(ID, device1));

	response.GetDevicesInfo(controller_manager->GetAllDevices(), result, command, "");
	EXPECT_TRUE(result.status());
	auto& devices1 = result.devices_info().devices();
	EXPECT_EQ(1, devices1.size());
	EXPECT_EQ(SCHS, devices1[0].type());
	EXPECT_EQ(ID, devices1[0].id());
	EXPECT_EQ(LUN1, devices1[0].unit());

	auto device2 = make_shared<MockSCSIHD_NEC>(LUN2);
	EXPECT_TRUE(controller_manager->AttachToScsiController(ID, device2));

	response.GetDevicesInfo(controller_manager->GetAllDevices(), result, command, "");
	EXPECT_TRUE(result.status());
	auto& devices2 = result.devices_info().devices();
	EXPECT_EQ(2, devices2.size()) << "Data for all devices must be returned";

	auto requested_device = command.add_devices();
	requested_device->set_id(ID);
	requested_device->set_unit(LUN1);
	response.GetDevicesInfo(controller_manager->GetAllDevices(), result, command, "");
	EXPECT_TRUE(result.status());
	auto& devices3 = result.devices_info().devices();
	EXPECT_EQ(1, devices3.size()) << "Only data for the specified ID and LUN must be returned";
	EXPECT_EQ(SCHS, devices3[0].type());
	EXPECT_EQ(ID, devices3[0].id());
	EXPECT_EQ(LUN1, devices3[0].unit());

	requested_device->set_id(ID);
	requested_device->set_unit(LUN3);
	response.GetDevicesInfo(controller_manager->GetAllDevices(), result, command, "");
	EXPECT_FALSE(result.status()) << "Only data for the specified ID and LUN must be returned";
}

TEST(PiscsiResponseTest, GetDeviceTypesInfo)
{
	PiscsiResponse response;
	PbResult result;

	const auto& info = response.GetDeviceTypesInfo(result);
	EXPECT_TRUE(result.status());
	EXPECT_EQ(8, info->properties().size());
}

TEST(PiscsiResponseTest, GetServerInfo)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	PiscsiResponse response;
	const unordered_set<shared_ptr<PrimaryDevice>> devices;
	const unordered_set<int> ids = { 1, 3 };
	PbResult result;

	const auto& info = response.GetServerInfo(devices, result, ids, "default_folder", "", "", 1234);
	EXPECT_TRUE(result.status());
	EXPECT_EQ(piscsi_major_version, info->version_info().major_version());
	EXPECT_EQ(piscsi_minor_version, info->version_info().minor_version());
	EXPECT_EQ(piscsi_patch_version, info->version_info().patch_version());
	EXPECT_EQ(level::level_string_views[get_level()], info->log_level_info().current_log_level());
	EXPECT_EQ("default_folder", info->image_files_info().default_image_folder());
	EXPECT_EQ(1234, info->image_files_info().depth());
	EXPECT_EQ(2, info->reserved_ids_info().ids().size());
}

TEST(PiscsiResponseTest, GetVersionInfo)
{
	PiscsiResponse response;
	PbResult result;

	const auto& info = response.GetVersionInfo(result);
	EXPECT_TRUE(result.status());
	EXPECT_EQ(piscsi_major_version, info->major_version());
	EXPECT_EQ(piscsi_minor_version, info->minor_version());
	EXPECT_EQ(piscsi_patch_version, info->patch_version());
}

TEST(PiscsiResponseTest, GetLogLevelInfo)
{
	PiscsiResponse response;
	PbResult result;

	const auto& info = response.GetLogLevelInfo(result);
	EXPECT_TRUE(result.status());
	EXPECT_EQ(level::level_string_views[get_level()], info->current_log_level());
	EXPECT_EQ(7, info->log_levels().size());
}

TEST(PiscsiResponseTest, GetNetworkInterfacesInfo)
{
	PiscsiResponse response;
	PbResult result;

	const auto& info = response.GetNetworkInterfacesInfo(result);
	EXPECT_TRUE(result.status());
	EXPECT_FALSE(info->name().empty());
}

TEST(PiscsiResponseTest, GetMappingInfo)
{
	PiscsiResponse response;
	PbResult result;

	const auto& info = response.GetMappingInfo(result);
	EXPECT_TRUE(result.status());
	EXPECT_EQ(10, info->mapping().size());
}

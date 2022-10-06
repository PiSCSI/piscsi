//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "controllers/controller_manager.h"
#include "devices/device_factory.h"
#include "rascsi_interface.pb.h"
#include "rascsi/rascsi_response.h"

using namespace rascsi_interface;

TEST(RascsiResponseTest, Operation_Count)
{
	MockBus bus;
	ControllerManager controller_manager(bus);
	DeviceFactory device_factory;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	PbResult pb_operation_info_result;

	const auto operation_info = rascsi_response.GetOperationInfo(pb_operation_info_result, 0);
	EXPECT_EQ(PbOperation_ARRAYSIZE - 1, operation_info->operations_size());
}

void TestNonDiskDevice(const string& name, int default_param_count)
{
	MockBus bus;
	ControllerManager controller_manager(bus);
	DeviceFactory device_factory;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);

	auto d = device_factory.CreateDevice(controller_manager, UNDEFINED, 0, name);
	controller_manager.AttachToScsiController(0, d);

	PbServerInfo server_info;
	rascsi_response.GetDevices(server_info, "image_folder");

	EXPECT_EQ(1, server_info.devices_info().devices().size());
	const auto& device = server_info.devices_info().devices()[0];
	EXPECT_FALSE(device.properties().read_only());
	EXPECT_FALSE(device.properties().protectable());
	EXPECT_FALSE(device.properties().stoppable());
	EXPECT_FALSE(device.properties().removable());
	EXPECT_FALSE(device.properties().lockable());
	EXPECT_EQ(0, device.params().size());
	EXPECT_EQ(32, device.properties().luns());
	EXPECT_EQ(0, device.block_size());
	EXPECT_EQ(0, device.block_count());
	EXPECT_EQ(default_param_count, device.properties().default_params().size());
	EXPECT_FALSE(device.properties().supports_file());
	if (default_param_count) {
		EXPECT_TRUE(device.properties().supports_params());
	}
	else {
		EXPECT_FALSE(device.properties().supports_params());
	}
}

TEST(RascsiResponseTest, GetDevice_Printer)
{
	TestNonDiskDevice("printer", 2);
}

TEST(RascsiResponseTest, GetDevice_HostServices)
{
	TestNonDiskDevice("services", 0);
}

TEST(RascsiResponseTest, GetReservedIds)
{
	MockBus bus;
	ControllerManager controller_manager(bus);
	DeviceFactory device_factory;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	unordered_set<int> ids;
	PbResult result;

	const auto& reserved_ids_info1 = rascsi_response.GetReservedIds(result, ids);
	EXPECT_TRUE(result.status());
	EXPECT_TRUE(reserved_ids_info1->ids().empty());

	ids.insert(3);
	const auto& reserved_ids_info2 = rascsi_response.GetReservedIds(result, ids);
	EXPECT_TRUE(result.status());
	EXPECT_EQ(1, reserved_ids_info2->ids().size());
	EXPECT_EQ(3, reserved_ids_info2->ids()[0]);
}

//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "testing.h"
#include "controllers/controller_manager.h"
#include "devices/device_factory.h"
#include "rascsi_interface.pb.h"
#include "rascsi_response.h"

using namespace rascsi_interface;

TEST(RascsiResponseTest, Operation_Count)
{
	MockBus bus;
	ControllerManager controller_manager(bus);
	DeviceFactory device_factory;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	PbResult pb_operation_info_result;

	const auto operation_info = unique_ptr<PbOperationInfo>(rascsi_response.GetOperationInfo(pb_operation_info_result, 0));
	EXPECT_EQ(PbOperation_ARRAYSIZE - 1, operation_info->operations_size());
}

TEST(RascsiResponseTest, GetDevice_Printer)
{
	MockBus bus;
	ControllerManager controller_manager(bus);
	DeviceFactory device_factory;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);

	auto d = device_factory.CreateDevice(controller_manager, UNDEFINED, 0, "printer");
	controller_manager.CreateScsiController(0, d);

	PbServerInfo server_info;
	rascsi_response.GetDevices(server_info, "image_folder");
	device_factory.DeleteDevices();

	EXPECT_EQ(1, server_info.devices_info().devices().size());
	const auto& device = server_info.devices_info().devices()[0];
	EXPECT_EQ(32, device.properties().luns());
	EXPECT_EQ(0, device.block_size());
	EXPECT_EQ(0, device.block_count());
	EXPECT_FALSE(device.properties().supports_file());
	EXPECT_TRUE(device.properties().supports_params());
	EXPECT_EQ(2, device.properties().default_params().size());
	EXPECT_FALSE(device.properties().read_only());
	EXPECT_FALSE(device.properties().protectable());
	EXPECT_FALSE(device.properties().stoppable());
	EXPECT_FALSE(device.properties().removable());
	EXPECT_FALSE(device.properties().lockable());
	EXPECT_EQ(0, device.params().size());

}

TEST(RascsiResponseTest, GetDevice_HostServices)
{
	MockBus bus;
	ControllerManager controller_manager(bus);
	DeviceFactory device_factory;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);

	auto d = device_factory.CreateDevice(controller_manager, UNDEFINED, 0, "services");
	controller_manager.CreateScsiController(0, d);

	PbServerInfo server_info;
	rascsi_response.GetDevices(server_info, "image_folder");
	device_factory.DeleteDevices();

	EXPECT_EQ(1, server_info.devices_info().devices().size());
	const auto& device = server_info.devices_info().devices()[0];
	EXPECT_EQ(32, device.properties().luns());
	EXPECT_EQ(0, device.block_size());
	EXPECT_EQ(0, device.block_count());
	EXPECT_FALSE(device.properties().supports_file());
	EXPECT_FALSE(device.properties().supports_params());
	EXPECT_EQ(0, device.properties().default_params().size());
	EXPECT_FALSE(device.properties().read_only());
	EXPECT_FALSE(device.properties().protectable());
	EXPECT_FALSE(device.properties().stoppable());
	EXPECT_FALSE(device.properties().removable());
	EXPECT_FALSE(device.properties().lockable());
	EXPECT_EQ(0, device.params().size());
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

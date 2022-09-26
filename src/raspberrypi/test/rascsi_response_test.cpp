//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "testing.h"
#include "devices/device_factory.h"
#include "rascsi_interface.pb.h"
#include "rascsi_response.h"
#include "rascsi_image.h"

using namespace rascsi_interface;

TEST(RascsiResponseTest, Operation_Count)
{
	DeviceFactory device_factory;
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(&device_factory, &rascsi_image);
	PbResult pb_operation_info_result;

	const auto operation_info = unique_ptr<PbOperationInfo>(rascsi_response.GetOperationInfo(pb_operation_info_result, 0));
	EXPECT_EQ(PbOperation_ARRAYSIZE - 1, operation_info->operations_size());
}

TEST(RascsiResponseTest, GetDevice_HostServices)
{
	DeviceFactory device_factory;
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(&device_factory, &rascsi_image);
	device_factory.CreateDevice(UNDEFINED, "services", -1);

	PbServerInfo server_info;
	rascsi_response.GetDevices(server_info);
	device_factory.DeleteAllDevices();

	EXPECT_EQ(1, server_info.devices_info().devices().size());
	const auto& host_services = server_info.devices_info().devices()[0];
	EXPECT_EQ(0, host_services.block_size());
	EXPECT_EQ(0, host_services.block_count());
	EXPECT_FALSE(host_services.properties().supports_file());
	EXPECT_FALSE(host_services.properties().read_only());
	EXPECT_FALSE(host_services.properties().protectable());
	EXPECT_FALSE(host_services.properties().stoppable());
	EXPECT_FALSE(host_services.properties().removable());
	EXPECT_FALSE(host_services.properties().lockable());
	EXPECT_EQ(0, host_services.params().size());
}

TEST(RascsiResponseTest, GetReservedIds)
{
	DeviceFactory device_factory;
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(&device_factory, &rascsi_image);
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

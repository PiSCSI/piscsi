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

	const list<PbDevice>& devices = { server_info.devices_info().devices().begin(),
			server_info.devices_info().devices().end() };
	EXPECT_EQ(1, devices.size());
	const auto& host_services = devices.front();
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

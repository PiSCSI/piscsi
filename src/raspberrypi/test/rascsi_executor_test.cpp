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
#include "rascsi_executor.h"

using namespace rascsi_interface;

TEST(RascsiExecutorTest, DetachAll)
{
	const int ID = 4;

	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager;
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, rascsi_image, 32);
	RascsiExecutor executor(bus, rascsi_response, rascsi_image, device_factory, controller_manager);

	PrimaryDevice *device = device_factory.CreateDevice(UNDEFINED, "services", ID);
	controller_manager.CreateScsiController(bus, device);
	EXPECT_NE(nullptr, controller_manager.FindController(4));
	EXPECT_FALSE(device_factory.GetAllDevices().empty());

	executor.DetachAll();
	EXPECT_EQ(nullptr, controller_manager.FindController(4));
	EXPECT_TRUE(device_factory.GetAllDevices().empty());
}

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
#include "controllers/controller_manager.h"

TEST(ControllerManagerTest, LifeCycle)
{
	const int ID = 4;
	const int LUN1 = 2;
	const int LUN2 = 3;
	DeviceFactory device_factory;
	MockBus bus;
	ControllerManager controller_manager(bus);

	auto device1 = device_factory.CreateDevice(UNDEFINED, ID, LUN1, "services");
	controller_manager.CreateScsiController(ID, device1);
	auto controller = controller_manager.FindController(ID);
	EXPECT_NE(nullptr, controller);
	EXPECT_EQ(1, controller->GetLunCount());
	EXPECT_NE(nullptr, controller_manager.IdentifyController(1 << ID));
	EXPECT_EQ(nullptr, controller_manager.IdentifyController(0));
	EXPECT_EQ(nullptr, controller_manager.FindController(0));
	EXPECT_EQ(device1, controller_manager.GetDeviceByIdAndLun(ID, LUN1));
	EXPECT_EQ(nullptr, controller_manager.GetDeviceByIdAndLun(0, 0));

	auto device2 = device_factory.CreateDevice(UNDEFINED, ID, LUN2, "services");
	controller_manager.CreateScsiController(ID, device2);
	EXPECT_FALSE(controller_manager.DeleteController(ID));

	controller_manager.DeleteAllControllers();
	device_factory.DeleteAllDevices();
	EXPECT_EQ(nullptr, controller_manager.FindController(ID));
	EXPECT_EQ(nullptr, controller_manager.GetDeviceByIdAndLun(ID, LUN1));
}

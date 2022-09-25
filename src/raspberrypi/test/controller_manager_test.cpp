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

TEST(ControllerManagerTest, ControllerManager)
{
	const int ID = 4;
	const int LUN = 6;
	DeviceFactory device_factory;
	ControllerManager controller_manager;

	auto device = device_factory.CreateDevice(UNDEFINED, "services", ID);
	device->SetId(ID);
	device->SetLun(LUN);

	controller_manager.CreateScsiController(nullptr, device);
	EXPECT_NE(nullptr, controller_manager.IdentifyController(1 << ID));
	EXPECT_EQ(nullptr, controller_manager.IdentifyController(0));
	EXPECT_NE(nullptr, controller_manager.FindController(ID));
	EXPECT_EQ(nullptr, controller_manager.FindController(0));
	EXPECT_EQ(device, controller_manager.GetDeviceByIdAndLun(ID, LUN));
	EXPECT_EQ(nullptr, controller_manager.GetDeviceByIdAndLun(0, 0));

	controller_manager.DeleteAllControllers();
	device_factory.DeleteAllDevices();
	EXPECT_EQ(nullptr, controller_manager.FindController(ID));
	EXPECT_EQ(nullptr, controller_manager.GetDeviceByIdAndLun(ID, LUN));
}

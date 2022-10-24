//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "devices/device_factory.h"
#include "controllers/controller_manager.h"

TEST(ControllerManagerTest, LifeCycle)
{
	const int ID = 4;
	const int LUN1 = 0;
	const int LUN2 = 3;

	auto bus_ptr = make_shared<MockBus>();
	ControllerManager controller_manager(bus_ptr);
	DeviceFactory device_factory;

	auto device = device_factory.CreateDevice(controller_manager, SCHS, -1, "");
	EXPECT_FALSE(controller_manager.AttachToScsiController(ID, device));

	device = device_factory.CreateDevice(controller_manager, SCHS, LUN1, "");
	EXPECT_TRUE(controller_manager.AttachToScsiController(ID, device));
	auto controller = controller_manager.FindController(ID);
	EXPECT_NE(nullptr, controller);
	EXPECT_EQ(1, controller->GetLunCount());
	EXPECT_NE(nullptr, controller_manager.IdentifyController(1 << ID));
	EXPECT_EQ(nullptr, controller_manager.IdentifyController(0));
	EXPECT_EQ(nullptr, controller_manager.FindController(0));
	EXPECT_NE(nullptr, controller_manager.GetDeviceByIdAndLun(ID, LUN1));
	EXPECT_EQ(nullptr, controller_manager.GetDeviceByIdAndLun(0, 0));

	device = device_factory.CreateDevice(controller_manager, SCHS, LUN2, "");
	EXPECT_TRUE(controller_manager.AttachToScsiController(ID, device));
	controller = controller_manager.FindController(ID);
	EXPECT_TRUE(controller_manager.DeleteController(controller));
	EXPECT_EQ(nullptr, controller_manager.FindController(ID));

	controller_manager.DeleteAllControllers();
	EXPECT_EQ(nullptr, controller_manager.FindController(ID));
	EXPECT_EQ(nullptr, controller_manager.GetDeviceByIdAndLun(ID, LUN1));
}

TEST(ControllerManagerTest, AttachToScsiController)
{
	const int ID = 4;
	const int LUN1 = 3;
	const int LUN2 = 0;

	auto bus_ptr = make_shared<MockBus>();
	ControllerManager controller_manager(bus_ptr);
	DeviceFactory device_factory;

	auto device1 = device_factory.CreateDevice(controller_manager, SCHS, LUN1, "");
	EXPECT_FALSE(controller_manager.AttachToScsiController(ID, device1)) << "LUN 0 is missing";

	auto device2 = device_factory.CreateDevice(controller_manager, SCLP, LUN2, "");
	EXPECT_TRUE(controller_manager.AttachToScsiController(ID, device2));
	EXPECT_TRUE(controller_manager.AttachToScsiController(ID, device1));
	EXPECT_FALSE(controller_manager.AttachToScsiController(ID, device1));
}

TEST(ControllerManagerTest, ResetAllControllers)
{
	const int ID = 2;

	auto bus_ptr = make_shared<MockBus>();
	ControllerManager controller_manager(bus_ptr);
	DeviceFactory device_factory;

	auto device = device_factory.CreateDevice(controller_manager, SCHS, 0, "");
	EXPECT_TRUE(controller_manager.AttachToScsiController(ID, device));
	auto controller = controller_manager.FindController(ID);
	EXPECT_NE(nullptr, controller);

	controller->SetStatus(status::RESERVATION_CONFLICT);
	controller_manager.ResetAllControllers();
	EXPECT_EQ(status::GOOD, controller->GetStatus());
}

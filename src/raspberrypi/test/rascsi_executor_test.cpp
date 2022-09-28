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
#include "rascsi_response.h"
#include "rascsi_image.h"
#include "rascsi_executor.h"

using namespace rascsi_interface;

TEST(RascsiExecutorTest, SetLogLevel)
{
	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager;
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, rascsi_image, 32);
	RascsiExecutor executor(bus, rascsi_response, rascsi_image, device_factory, controller_manager);

	EXPECT_TRUE(executor.SetLogLevel("trace"));
	EXPECT_TRUE(executor.SetLogLevel("debug"));
	EXPECT_TRUE(executor.SetLogLevel("info"));
	EXPECT_TRUE(executor.SetLogLevel("warn"));
	EXPECT_TRUE(executor.SetLogLevel("err"));
	EXPECT_TRUE(executor.SetLogLevel("critical"));
	EXPECT_TRUE(executor.SetLogLevel("off"));
	EXPECT_FALSE(executor.SetLogLevel("xyz"));
}

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

TEST(RascsiExecutorTest, SetReservedIds)
{
	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager;
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, rascsi_image, 32);
	RascsiExecutor executor(bus, rascsi_response, rascsi_image, device_factory, controller_manager);

	string error = executor.SetReservedIds("xyz");
	EXPECT_FALSE(error.empty());
	EXPECT_TRUE(executor.GetReservedIds().empty());

	error = executor.SetReservedIds("8");
	EXPECT_FALSE(error.empty());
	EXPECT_TRUE(executor.GetReservedIds().empty());

	error = executor.SetReservedIds("-1");
	EXPECT_FALSE(error.empty());
	EXPECT_TRUE(executor.GetReservedIds().empty());

	error = executor.SetReservedIds("");
	EXPECT_TRUE(error.empty());
	EXPECT_TRUE(executor.GetReservedIds().empty());

	error = executor.SetReservedIds("7,1,2,3,5");
	EXPECT_TRUE(error.empty());
	unordered_set<int> reserved_ids = executor.GetReservedIds();
	EXPECT_EQ(5, reserved_ids.size());
	EXPECT_NE(reserved_ids.end(), reserved_ids.find(1));
	EXPECT_NE(reserved_ids.end(), reserved_ids.find(2));
	EXPECT_NE(reserved_ids.end(), reserved_ids.find(3));
	EXPECT_NE(reserved_ids.end(), reserved_ids.find(5));
	EXPECT_NE(reserved_ids.end(), reserved_ids.find(7));
}
TEST(RascsiExecutorTest, ValidateLunSetup)
{
	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager;
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, rascsi_image, 32);
	RascsiExecutor executor(bus, rascsi_response, rascsi_image, device_factory, controller_manager);
	PbCommand command;

	PbDeviceDefinition *device = command.add_devices();
	device->set_unit(0);
	string error = executor.ValidateLunSetup(command);
	EXPECT_TRUE(error.empty());

	device->set_unit(1);
	error = executor.ValidateLunSetup(command);
	EXPECT_FALSE(error.empty());

	device_factory.CreateDevice(UNDEFINED, "services", 0);
	error = executor.ValidateLunSetup(command);
	EXPECT_TRUE(error.empty());
	device_factory.DeleteAllDevices();
}

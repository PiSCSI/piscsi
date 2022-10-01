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
#include "localizer.h"
#include "command_util.h"
#include "rascsi_response.h"
#include "rascsi_image.h"
#include "rascsi_executor.h"

using namespace rascsi_interface;
using namespace command_util;

TEST(RascsiExecutorTest, SetLogLevel)
{
	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager(bus);
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	RascsiExecutor executor(rascsi_response, rascsi_image, device_factory, controller_manager);

	EXPECT_TRUE(executor.SetLogLevel("trace"));
	EXPECT_TRUE(executor.SetLogLevel("debug"));
	EXPECT_TRUE(executor.SetLogLevel("info"));
	EXPECT_TRUE(executor.SetLogLevel("warn"));
	EXPECT_TRUE(executor.SetLogLevel("err"));
	EXPECT_TRUE(executor.SetLogLevel("critical"));
	EXPECT_TRUE(executor.SetLogLevel("off"));
	EXPECT_FALSE(executor.SetLogLevel("xyz"));
}

TEST(RascsiExecutorTest, Attach)
{
	const int ID = 3;
	const int LUN = 0;

	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager(bus);
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	RascsiExecutor executor(rascsi_response, rascsi_image, device_factory, controller_manager);
	PbDeviceDefinition device_definition;
	ProtobufSerializer serializer;
	Localizer localizer;
	CommandContext context(serializer, localizer, STDOUT_FILENO, "");

	device_definition.set_unit(32);
	EXPECT_FALSE(executor.Attach(context, device_definition, false));

	auto device = device_factory.CreateDevice(controller_manager, UNDEFINED, LUN, "services");
	controller_manager.AttachToScsiController(ID, device);
	device_definition.set_id(ID);
	device_definition.set_unit(LUN);
	EXPECT_FALSE(executor.Attach(context, device_definition, false));
	controller_manager.DeleteAllControllers();

	device_definition.set_type(PbDeviceType::SCHS);
	EXPECT_TRUE(executor.Attach(context, device_definition, false));
	controller_manager.DeleteAllControllers();

	device_definition.set_type(PbDeviceType::SCHD);
	EXPECT_FALSE(executor.Attach(context, device_definition, false));

	device_definition.set_block_size(1);
	EXPECT_FALSE(executor.Attach(context, device_definition, false)) << "Invalid sector size";

	device_definition.set_block_size(1024);
	EXPECT_FALSE(executor.Attach(context, device_definition, false));

	AddParam(device_definition, "file", "/non_existing_file");
	EXPECT_FALSE(executor.Attach(context, device_definition, false));

	AddParam(device_definition, "file", "/dev/zero");
	EXPECT_FALSE(executor.Attach(context, device_definition, false)) << "File has 0 bytes";
}

TEST(RascsiExecutorTest, Detach)
{
	const int ID = 3;
	const int LUN1 = 0;
	const int LUN2 = 1;

	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager(bus);
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	RascsiExecutor executor(rascsi_response, rascsi_image, device_factory, controller_manager);
	ProtobufSerializer serializer;
	Localizer localizer;
	CommandContext context(serializer, localizer, STDOUT_FILENO, "");

	auto device1 = device_factory.CreateDevice(controller_manager, UNDEFINED, LUN1, "services");
	controller_manager.AttachToScsiController(ID, device1);
	auto device2 = device_factory.CreateDevice(controller_manager, UNDEFINED, LUN2, "services");
	controller_manager.AttachToScsiController(ID, device2);

	auto d1 = controller_manager.GetDeviceByIdAndLun(ID, LUN1);
	EXPECT_FALSE(executor.Detach(context, *d1, false)) << "LUNs > 0 have to be detached first";
	auto d2 = controller_manager.GetDeviceByIdAndLun(ID, LUN2);
	EXPECT_TRUE(executor.Detach(context, *d2, false));
	EXPECT_TRUE(executor.Detach(context, *d1, false));
	EXPECT_TRUE(controller_manager.GetAllDevices().empty());
}

TEST(RascsiExecutorTest, DetachAll)
{
	const int ID = 4;

	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager(bus);
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	RascsiExecutor executor(rascsi_response, rascsi_image, device_factory, controller_manager);

	auto device = device_factory.CreateDevice(controller_manager, UNDEFINED, 0, "services");
	controller_manager.AttachToScsiController(ID, device);
	EXPECT_NE(nullptr, controller_manager.FindController(ID));
	EXPECT_FALSE(controller_manager.GetAllDevices().empty());

	executor.DetachAll();
	EXPECT_EQ(nullptr, controller_manager.FindController(ID));
	EXPECT_TRUE(controller_manager.GetAllDevices().empty());
}

TEST(RascsiExecutorTest, ShutDown)
{
	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager(bus);
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	RascsiExecutor executor(rascsi_response, rascsi_image, device_factory, controller_manager);
	ProtobufSerializer serializer;
	Localizer localizer;
	CommandContext context(serializer, localizer, STDOUT_FILENO, "");

	EXPECT_FALSE(executor.ShutDown(context, ""));
	EXPECT_FALSE(executor.ShutDown(context, "xyz"));
	EXPECT_TRUE(executor.ShutDown(context, "rascsi"));
	EXPECT_FALSE(executor.ShutDown(context, "system"));
	EXPECT_FALSE(executor.ShutDown(context, "reboot"));
}

TEST(RascsiExecutorTest, SetReservedIds)
{
	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager(bus);
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	RascsiExecutor executor(rascsi_response, rascsi_image, device_factory, controller_manager);

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

	auto device = device_factory.CreateDevice(controller_manager, UNDEFINED, 0, "services");
	controller_manager.AttachToScsiController(5, device);
	error = executor.SetReservedIds("5");
	EXPECT_FALSE(error.empty());
}

TEST(RascsiExecutorTest, ValidateLunSetup)
{
	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager(bus);
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	RascsiExecutor executor(rascsi_response, rascsi_image, device_factory, controller_manager);
	PbCommand command;

	auto device1 = command.add_devices();
	device1->set_unit(0);
	string error = executor.ValidateLunSetup(command);
	EXPECT_TRUE(error.empty());

	device1->set_unit(1);
	error = executor.ValidateLunSetup(command);
	EXPECT_FALSE(error.empty());

	auto device2 = device_factory.CreateDevice(controller_manager, UNDEFINED, 0, "services");
	controller_manager.AttachToScsiController(0, device2);
	error = executor.ValidateLunSetup(command);
	EXPECT_TRUE(error.empty());
}
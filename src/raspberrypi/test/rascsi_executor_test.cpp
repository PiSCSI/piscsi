//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "spdlog/spdlog.h"
#include "mocks.h"
#include "rascsi_exceptions.h"
#include "protobuf_util.h"
#include "controllers/controller_manager.h"
#include "devices/device_factory.h"
#include "rascsi_interface.pb.h"
#include "rascsi/command_context.h"
#include "rascsi/rascsi_response.h"
#include "rascsi/rascsi_image.h"
#include "rascsi/rascsi_executor.h"
#include <unistd.h>

using namespace rascsi_interface;
using namespace protobuf_util;

const extern bool enable_logging;

// This test fixture is required in order to reset the log level changed by the log level tests
class RascsiExecutorTest : public Test
{
	void TearDown() override {
		spdlog::set_level(enable_logging ? spdlog::level::trace : spdlog::level::off);
	}
};

TEST_F(RascsiExecutorTest, ProcessDeviceCmd)
{
	const int ID = 3;
	const int LUN = 0;

	MockBus bus;
	DeviceFactory device_factory;
	MockAbstractController controller(ID);
	ControllerManager controller_manager(bus);
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	auto executor = make_shared<MockRascsiExecutor>(rascsi_response, rascsi_image, device_factory, controller_manager);
	PbDeviceDefinition definition;
	PbCommand command;
	MockCommandContext context;

	definition.set_id(8);
	definition.set_unit(32);
	EXPECT_FALSE(executor->ProcessDeviceCmd(context, definition, command, true)) << "Invalid ID and LUN must fail";

	definition.set_unit(LUN);
	EXPECT_FALSE(executor->ProcessDeviceCmd(context, definition, command, true)) << "Invalid ID must fail";

	definition.set_id(ID);
	definition.set_unit(32);
	EXPECT_FALSE(executor->ProcessDeviceCmd(context, definition, command, true)) << "Invalid LUN must fail";

	definition.set_unit(LUN);
	EXPECT_FALSE(executor->ProcessDeviceCmd(context, definition, command, true)) << "Unknown operation must fail";

	command.set_operation(ATTACH);
	EXPECT_FALSE(executor->ProcessDeviceCmd(context, definition, command, true)) << "Operation for unknown device type must fail";

	auto device1 = make_shared<MockPrimaryDevice>(LUN);
	EXPECT_TRUE(controller_manager.AttachToScsiController(ID, device1));

	definition.set_type(SCHS);
	command.set_operation(INSERT);
	EXPECT_FALSE(executor->ProcessDeviceCmd(context, definition, command, true)) << "Operation unsupported by device must fail";
	controller_manager.DeleteAllControllers();
	definition.set_type(SCRM);

	auto device2 = make_shared<MockSCSIHD_NEC>(LUN);
	device2->SetRemovable(true);
	device2->SetProtectable(true);
	device2->SetReady(true);
	EXPECT_TRUE(controller_manager.AttachToScsiController(ID, device2));

	command.set_operation(ATTACH);
	EXPECT_FALSE(executor->ProcessDeviceCmd(context, definition, command, true)) << "ID and LUN already exist";

	command.set_operation(START);
	EXPECT_TRUE(executor->ProcessDeviceCmd(context, definition, command, true));
	EXPECT_TRUE(executor->ProcessDeviceCmd(context, definition, command, false));

	command.set_operation(PROTECT);
	EXPECT_TRUE(executor->ProcessDeviceCmd(context, definition, command, true));
	EXPECT_TRUE(executor->ProcessDeviceCmd(context, definition, command, false));

	command.set_operation(UNPROTECT);
	EXPECT_TRUE(executor->ProcessDeviceCmd(context, definition, command, true));
	EXPECT_TRUE(executor->ProcessDeviceCmd(context, definition, command, false));

	command.set_operation(STOP);
	EXPECT_TRUE(executor->ProcessDeviceCmd(context, definition, command, true));
	EXPECT_TRUE(executor->ProcessDeviceCmd(context, definition, command, false));

	command.set_operation(EJECT);
	EXPECT_TRUE(executor->ProcessDeviceCmd(context, definition, command, true));
	EXPECT_TRUE(executor->ProcessDeviceCmd(context, definition, command, false));

	command.set_operation(INSERT);
	SetParam(definition, "file", "filename");
	EXPECT_FALSE(executor->ProcessDeviceCmd(context, definition, command, true)) << "Non-existing file";
	EXPECT_FALSE(executor->ProcessDeviceCmd(context, definition, command, false)) << "Non-existing file";

	command.set_operation(DETACH);
	EXPECT_TRUE(executor->ProcessDeviceCmd(context, definition, command, true));
	EXPECT_TRUE(executor->ProcessDeviceCmd(context, definition, command, false));
	EXPECT_TRUE(controller_manager.AttachToScsiController(ID, device2));

	command.set_operation(CHECK_AUTHENTICATION);
	EXPECT_TRUE(executor->ProcessDeviceCmd(context, definition, command, true));
	EXPECT_TRUE(executor->ProcessDeviceCmd(context, definition, command, false));

	command.set_operation(NO_OPERATION);
	EXPECT_TRUE(executor->ProcessDeviceCmd(context, definition, command, true));
	EXPECT_TRUE(executor->ProcessDeviceCmd(context, definition, command, false));

	// The operations below are not related to a device

	command.set_operation(DETACH_ALL);
	EXPECT_FALSE(executor->ProcessDeviceCmd(context, definition, command, true));

	command.set_operation(RESERVE_IDS);
	EXPECT_FALSE(executor->ProcessDeviceCmd(context, definition, command, true));

	command.set_operation(CREATE_IMAGE);
	EXPECT_FALSE(executor->ProcessDeviceCmd(context, definition, command, true));

	command.set_operation(DELETE_IMAGE);
	EXPECT_FALSE(executor->ProcessDeviceCmd(context, definition, command, true));

	command.set_operation(RENAME_IMAGE);
	EXPECT_FALSE(executor->ProcessDeviceCmd(context, definition, command, true));

	command.set_operation(COPY_IMAGE);
	EXPECT_FALSE(executor->ProcessDeviceCmd(context, definition, command, true));

	command.set_operation(PROTECT_IMAGE);
	EXPECT_FALSE(executor->ProcessDeviceCmd(context, definition, command, true));

	command.set_operation(UNPROTECT_IMAGE);
	EXPECT_FALSE(executor->ProcessDeviceCmd(context, definition, command, true));
}

TEST_F(RascsiExecutorTest, ProcessCmd)
{
	MockBus bus;
	DeviceFactory device_factory;
	MockAbstractController controller(0);
	ControllerManager controller_manager(bus);
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	auto executor = make_shared<MockRascsiExecutor>(rascsi_response, rascsi_image, device_factory, controller_manager);
	PbCommand command1;
	PbCommand command2;
	MockCommandContext context;

	command1.set_operation(DETACH_ALL);
	EXPECT_TRUE(executor->ProcessCmd(context, command1));

	command1.set_operation(RESERVE_IDS);
	SetParam(command1, "ids", "2,3");
	EXPECT_TRUE(executor->ProcessCmd(context, command1));
	const unordered_set<int> ids = executor->GetReservedIds();
	EXPECT_EQ(2, ids.size());
	EXPECT_NE(ids.end(), ids.find(2));
	EXPECT_NE(ids.end(), ids.find(3));
	command2.set_operation(RESERVE_IDS);
	EXPECT_TRUE(executor->ProcessCmd(context, command2));
	EXPECT_TRUE(executor->GetReservedIds().empty());

	SetParam(command2, "ids", "-1");
	EXPECT_FALSE(executor->ProcessCmd(context, command2));
	EXPECT_TRUE(executor->GetReservedIds().empty());

	command1.set_operation(NO_OPERATION);
	EXPECT_TRUE(executor->ProcessCmd(context, command1));

	command1.set_operation(ATTACH);
	auto device = command1.add_devices();
	device->set_type(SCHS);
	device->set_id(-1);
	EXPECT_FALSE(executor->ProcessCmd(context, command1));
	device->set_id(0);
	device->set_unit(1);
	EXPECT_FALSE(executor->ProcessCmd(context, command1)) << "LUN 0 is missing";
	device->set_unit(0);
	EXPECT_TRUE(executor->ProcessCmd(context, command1));

	// The operations below must fail because of missing parameters.
	// The respective functionality is tested in rascsi_image_test.cpp.

	command1.set_operation(CREATE_IMAGE);
	EXPECT_FALSE(executor->ProcessCmd(context, command1));

	command1.set_operation(DELETE_IMAGE);
	EXPECT_FALSE(executor->ProcessCmd(context, command1));

	command1.set_operation(RENAME_IMAGE);
	EXPECT_FALSE(executor->ProcessCmd(context, command1));

	command1.set_operation(COPY_IMAGE);
	EXPECT_FALSE(executor->ProcessCmd(context, command1));

	command1.set_operation(PROTECT_IMAGE);
	EXPECT_FALSE(executor->ProcessCmd(context, command1));

	command1.set_operation(UNPROTECT_IMAGE);
	EXPECT_FALSE(executor->ProcessCmd(context, command1));
}

TEST_F(RascsiExecutorTest, SetLogLevel)
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
	EXPECT_TRUE(executor.SetLogLevel("off"));
	EXPECT_FALSE(executor.SetLogLevel("xyz"));
}

TEST_F(RascsiExecutorTest, Attach)
{
	const int ID = 3;
	const int LUN = 0;

	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager(bus);
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	RascsiExecutor executor(rascsi_response, rascsi_image, device_factory, controller_manager);
	PbDeviceDefinition definition;
	MockCommandContext context;

	definition.set_unit(32);
	EXPECT_FALSE(executor.Attach(context, definition, false));

	auto device = device_factory.CreateDevice(controller_manager, SCHD, LUN, "");
	definition.set_id(ID);
	definition.set_unit(LUN);

	executor.SetReservedIds(to_string(ID));
	EXPECT_FALSE(executor.Attach(context, definition, false)) << "Reserved ID not rejected";

	executor.SetReservedIds("");
	EXPECT_FALSE(executor.Attach(context, definition, false)) << "Unknown device type not rejected";

	definition.set_type(PbDeviceType::SCHS);
	EXPECT_TRUE(executor.Attach(context, definition, false));
	controller_manager.DeleteAllControllers();

	definition.set_type(PbDeviceType::SCHD);
	EXPECT_FALSE(executor.Attach(context, definition, false)) << "Drive without sectors not rejected";

	definition.set_revision("invalid revision");
	EXPECT_FALSE(executor.Attach(context, definition, false)) << "Drive with invalid revision not rejected";
	definition.set_revision("1234");

	definition.set_block_size(1);
	EXPECT_FALSE(executor.Attach(context, definition, false)) << "Drive with invalid sector size not rejected";

	definition.set_block_size(512);
	EXPECT_FALSE(executor.Attach(context, definition, false)) << "Drive without image file not rejected";

	SetParam(definition, "file", "/non_existing_file");
	EXPECT_FALSE(executor.Attach(context, definition, false)) << "Drive with non-existing image file not rejected";

	string filename = CreateTempFile(1);
	SetParam(definition, "file", filename);
	EXPECT_THROW(executor.Attach(context, definition, false), io_exception)	<< "Too small image file not rejected";
	unlink(filename.c_str());

	filename = CreateTempFile(512);
	SetParam(definition, "file", filename);
	bool result = executor.Attach(context, definition, false);
	unlink(filename.c_str());
	EXPECT_TRUE(result);
	controller_manager.DeleteAllControllers();

	filename = CreateTempFile(513);
	SetParam(definition, "file", filename);
	result = executor.Attach(context, definition, false);
	unlink(filename.c_str());
	EXPECT_TRUE(result);

	definition.set_type(PbDeviceType::SCCD);
	definition.set_unit(LUN + 1);
	filename = CreateTempFile(2048);
	SetParam(definition, "file", filename);
	result = executor.Attach(context, definition, false);
	unlink(filename.c_str());
	EXPECT_TRUE(result);

	definition.set_type(PbDeviceType::SCMO);
	definition.set_unit(LUN + 2);
	SetParam(definition, "read_only", "true");
	filename = CreateTempFile(4096);
	SetParam(definition, "file", filename);
	result = executor.Attach(context, definition, false);
	unlink(filename.c_str());
	EXPECT_TRUE(result);

	controller_manager.DeleteAllControllers();
}

TEST_F(RascsiExecutorTest, Insert)
{
	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager(bus);
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	RascsiExecutor executor(rascsi_response, rascsi_image, device_factory, controller_manager);
	PbDeviceDefinition definition;
	MockCommandContext context;

	auto device = device_factory.CreateDevice(controller_manager, SCRM, 0, "test");

	device->SetRemoved(false);
	EXPECT_FALSE(executor.Insert(context, definition, device, false)) << "Medium is not removed";

	device->SetRemoved(true);
	definition.set_vendor("v");
	EXPECT_FALSE(executor.Insert(context, definition, device, false)) << "Product data must not be set";
	definition.set_vendor("");

	definition.set_product("p");
	EXPECT_FALSE(executor.Insert(context, definition, device, false)) << "Product data must not be set";
	definition.set_product("");

	definition.set_revision("r");
	EXPECT_FALSE(executor.Insert(context, definition, device, false)) << "Product data must not be set";
	definition.set_revision("");

	EXPECT_FALSE(executor.Insert(context, definition, device, false)) << "Filename is missing";

	SetParam(definition, "file", "filename");
	EXPECT_TRUE(executor.Insert(context, definition, device, true)) << "Dry-run must not fail";
	EXPECT_FALSE(executor.Insert(context, definition, device, false));

	definition.set_block_size(1);
	EXPECT_FALSE(executor.Insert(context, definition, device, false));

	definition.set_block_size(0);
	EXPECT_FALSE(executor.Insert(context, definition, device, false)) << "Image file validation must fail";

	SetParam(definition, "file", "/non_existing_file");
	EXPECT_FALSE(executor.Insert(context, definition, device, false));

	string filename = CreateTempFile(1);
	SetParam(definition, "file", filename);
	EXPECT_THROW(executor.Insert(context, definition, device, false), io_exception)
		<< "Too small image file not rejected";
	unlink(filename.c_str());

	filename = CreateTempFile(512);
	SetParam(definition, "file", filename);
	const bool result = executor.Insert(context, definition, device, false);
	unlink(filename.c_str());
	EXPECT_TRUE(result);
}

TEST_F(RascsiExecutorTest, Detach)
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
	MockCommandContext context;

	auto device1 = device_factory.CreateDevice(controller_manager, SCHS, LUN1, "");
	EXPECT_TRUE(controller_manager.AttachToScsiController(ID, device1));
	auto device2 = device_factory.CreateDevice(controller_manager, SCHS, LUN2, "");
	EXPECT_TRUE(controller_manager.AttachToScsiController(ID, device2));

	auto d1 = controller_manager.GetDeviceByIdAndLun(ID, LUN1);
	EXPECT_FALSE(executor.Detach(context, d1, false)) << "LUNs > 0 have to be detached first";
	auto d2 = controller_manager.GetDeviceByIdAndLun(ID, LUN2);
	EXPECT_TRUE(executor.Detach(context, d2, false));
	EXPECT_TRUE(executor.Detach(context, d1, false));
	EXPECT_TRUE(controller_manager.GetAllDevices().empty());

	EXPECT_FALSE(executor.Detach(context, d1, false));
}

TEST_F(RascsiExecutorTest, DetachAll)
{
	const int ID = 4;

	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager(bus);
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	RascsiExecutor executor(rascsi_response, rascsi_image, device_factory, controller_manager);

	auto device = device_factory.CreateDevice(controller_manager, SCHS, 0, "");
	EXPECT_TRUE(controller_manager.AttachToScsiController(ID, device));
	EXPECT_NE(nullptr, controller_manager.FindController(ID));
	EXPECT_FALSE(controller_manager.GetAllDevices().empty());

	executor.DetachAll();
	EXPECT_EQ(nullptr, controller_manager.FindController(ID));
	EXPECT_TRUE(controller_manager.GetAllDevices().empty());
}

TEST_F(RascsiExecutorTest, ShutDown)
{
	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager(bus);
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	RascsiExecutor executor(rascsi_response, rascsi_image, device_factory, controller_manager);
	MockCommandContext context;

	EXPECT_FALSE(executor.ShutDown(context, ""));
	EXPECT_FALSE(executor.ShutDown(context, "xyz"));
	EXPECT_TRUE(executor.ShutDown(context, "rascsi"));
	EXPECT_FALSE(executor.ShutDown(context, "system"));
	EXPECT_FALSE(executor.ShutDown(context, "reboot"));
}

TEST_F(RascsiExecutorTest, SetReservedIds)
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

	auto device = device_factory.CreateDevice(controller_manager, SCHS, 0, "");
	EXPECT_TRUE(controller_manager.AttachToScsiController(5, device));
	error = executor.SetReservedIds("5");
	EXPECT_FALSE(error.empty());
}

TEST_F(RascsiExecutorTest, ValidateImageFile)
{
	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager(bus);
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	RascsiExecutor executor(rascsi_response, rascsi_image, device_factory, controller_manager);
	MockCommandContext context;

	string full_path;
	auto device = dynamic_pointer_cast<StorageDevice>(device_factory.CreateDevice(controller_manager, SCHD, 0, "test"));
	EXPECT_TRUE(executor.ValidateImageFile(context, device, "", full_path));
	EXPECT_TRUE(full_path.empty());

	EXPECT_FALSE(executor.ValidateImageFile(context, device, "/non_existing_file", full_path));
	EXPECT_TRUE(full_path.empty());
}

TEST_F(RascsiExecutorTest, ValidateLunSetup)
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

	auto device2 = device_factory.CreateDevice(controller_manager, SCHS, 0, "");
	EXPECT_TRUE(controller_manager.AttachToScsiController(0, device2));
	error = executor.ValidateLunSetup(command);
	EXPECT_TRUE(error.empty());
}

TEST_F(RascsiExecutorTest, VerifyExistingIdAndLun)
{
	const int ID = 1;
	const int LUN1 = 0;
	const int LUN2 = 3;

	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager(bus);
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	RascsiExecutor executor(rascsi_response, rascsi_image, device_factory, controller_manager);
	MockCommandContext context;

	EXPECT_FALSE(executor.VerifyExistingIdAndLun(context, ID, LUN1));
	auto device = device_factory.CreateDevice(controller_manager, SCHS, LUN1, "");
	EXPECT_TRUE(controller_manager.AttachToScsiController(ID, device));
	EXPECT_TRUE(executor.VerifyExistingIdAndLun(context, ID, LUN1));
	EXPECT_FALSE(executor.VerifyExistingIdAndLun(context, ID, LUN2));
}

TEST_F(RascsiExecutorTest, CreateDevice)
{
	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager(bus);
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	RascsiExecutor executor(rascsi_response, rascsi_image, device_factory, controller_manager);
	MockCommandContext context;

	EXPECT_EQ(nullptr, executor.CreateDevice(context, UNDEFINED, 0, ""));
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
	EXPECT_EQ(nullptr, executor.CreateDevice(context, SAHD, 0, ""));
#pragma GCC diagnostic pop
	EXPECT_NE(nullptr, executor.CreateDevice(context, UNDEFINED, 0, "services"));
	EXPECT_NE(nullptr, executor.CreateDevice(context, SCHS, 0, ""));
}

TEST_F(RascsiExecutorTest, SetSectorSize)
{
	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager(bus);
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	RascsiExecutor executor(rascsi_response, rascsi_image, device_factory, controller_manager);
	MockCommandContext context;

	unordered_set<uint32_t> sizes;
	auto hd = make_shared<MockSCSIHD>(0, sizes, false);
	EXPECT_FALSE(executor.SetSectorSize(context, hd, 512));

	sizes.insert(512);
	hd = make_shared<MockSCSIHD>(0, sizes, false);
	EXPECT_TRUE(executor.SetSectorSize(context, hd, 0));
	EXPECT_FALSE(executor.SetSectorSize(context, hd, 1));
	EXPECT_TRUE(executor.SetSectorSize(context, hd, 512));
}

TEST_F(RascsiExecutorTest, ValidateOperationAgainstDevice)
{
	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager(bus);
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	RascsiExecutor executor(rascsi_response, rascsi_image, device_factory, controller_manager);
	MockCommandContext context;

	auto device = make_shared<MockPrimaryDevice>(0);

	EXPECT_TRUE(executor.ValidateOperationAgainstDevice(context, device, ATTACH));
	EXPECT_TRUE(executor.ValidateOperationAgainstDevice(context, device, DETACH));

	EXPECT_FALSE(executor.ValidateOperationAgainstDevice(context, device, START));
	EXPECT_FALSE(executor.ValidateOperationAgainstDevice(context, device, STOP));
	EXPECT_FALSE(executor.ValidateOperationAgainstDevice(context, device, INSERT));
	EXPECT_FALSE(executor.ValidateOperationAgainstDevice(context, device, EJECT));
	EXPECT_FALSE(executor.ValidateOperationAgainstDevice(context, device, PROTECT));
	EXPECT_FALSE(executor.ValidateOperationAgainstDevice(context, device, UNPROTECT));

	device->SetStoppable(true);
	EXPECT_TRUE(executor.ValidateOperationAgainstDevice(context, device, START));
	EXPECT_TRUE(executor.ValidateOperationAgainstDevice(context, device, STOP));
	EXPECT_FALSE(executor.ValidateOperationAgainstDevice(context, device, INSERT));
	EXPECT_FALSE(executor.ValidateOperationAgainstDevice(context, device, EJECT));
	EXPECT_FALSE(executor.ValidateOperationAgainstDevice(context, device, PROTECT));
	EXPECT_FALSE(executor.ValidateOperationAgainstDevice(context, device, UNPROTECT));

	device->SetRemovable(true);
	EXPECT_TRUE(executor.ValidateOperationAgainstDevice(context, device, START));
	EXPECT_TRUE(executor.ValidateOperationAgainstDevice(context, device, STOP));
	EXPECT_TRUE(executor.ValidateOperationAgainstDevice(context, device, INSERT));
	EXPECT_TRUE(executor.ValidateOperationAgainstDevice(context, device, EJECT));
	EXPECT_FALSE(executor.ValidateOperationAgainstDevice(context, device, PROTECT));
	EXPECT_FALSE(executor.ValidateOperationAgainstDevice(context, device, UNPROTECT));

	device->SetProtectable(true);
	EXPECT_TRUE(executor.ValidateOperationAgainstDevice(context, device, START));
	EXPECT_TRUE(executor.ValidateOperationAgainstDevice(context, device, STOP));
	EXPECT_TRUE(executor.ValidateOperationAgainstDevice(context, device, INSERT));
	EXPECT_TRUE(executor.ValidateOperationAgainstDevice(context, device, EJECT));
	EXPECT_FALSE(executor.ValidateOperationAgainstDevice(context, device, PROTECT));
	EXPECT_FALSE(executor.ValidateOperationAgainstDevice(context, device, UNPROTECT));

	device->SetReady(true);
	EXPECT_TRUE(executor.ValidateOperationAgainstDevice(context, device, START));
	EXPECT_TRUE(executor.ValidateOperationAgainstDevice(context, device, STOP));
	EXPECT_TRUE(executor.ValidateOperationAgainstDevice(context, device, INSERT));
	EXPECT_TRUE(executor.ValidateOperationAgainstDevice(context, device, EJECT));
	EXPECT_TRUE(executor.ValidateOperationAgainstDevice(context, device, PROTECT));
	EXPECT_TRUE(executor.ValidateOperationAgainstDevice(context, device, UNPROTECT));
}

TEST_F(RascsiExecutorTest, ValidateIdAndLun)
{
	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager(bus);
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	RascsiExecutor executor(rascsi_response, rascsi_image, device_factory, controller_manager);
	MockCommandContext context;

	EXPECT_FALSE(executor.ValidateIdAndLun(context, -1, 0));
	EXPECT_FALSE(executor.ValidateIdAndLun(context, 8, 0));
	EXPECT_FALSE(executor.ValidateIdAndLun(context, 7, -1));
	EXPECT_FALSE(executor.ValidateIdAndLun(context, 7, 32));
	EXPECT_TRUE(executor.ValidateIdAndLun(context, 7, 0));
	EXPECT_TRUE(executor.ValidateIdAndLun(context, 7, 31));
}

TEST_F(RascsiExecutorTest, SetProductData)
{
	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager(bus);
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	RascsiExecutor executor(rascsi_response, rascsi_image, device_factory, controller_manager);
	MockCommandContext context;
	PbDeviceDefinition definition;

	auto device = make_shared<MockPrimaryDevice>(0);

	EXPECT_TRUE(executor.SetProductData(context, definition, device));

	definition.set_vendor("123456789");
	EXPECT_FALSE(executor.SetProductData(context, definition, device));
	definition.set_vendor("1");
	EXPECT_TRUE(executor.SetProductData(context, definition, device));
	definition.set_vendor("12345678");
	EXPECT_TRUE(executor.SetProductData(context, definition, device));

	definition.set_product("12345678901234567");
	EXPECT_FALSE(executor.SetProductData(context, definition, device));
	definition.set_product("1");
	EXPECT_TRUE(executor.SetProductData(context, definition, device));
	definition.set_product("1234567890123456");
	EXPECT_TRUE(executor.SetProductData(context, definition, device));

	definition.set_revision("12345");
	EXPECT_FALSE(executor.SetProductData(context, definition, device));
	definition.set_revision("1");
	EXPECT_TRUE(executor.SetProductData(context, definition, device));
	definition.set_revision("1234");
	EXPECT_TRUE(executor.SetProductData(context, definition, device));
}

//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "rascsi_exceptions.h"
#include "controllers/controller_manager.h"
#include "devices/host_services.h"

using namespace std;

TEST(HostServicesTest, TestUnitReady)
{
	MockBus bus;
	ControllerManager controller_manager(bus);
    MockAbstractController controller(0);
	auto device = make_shared<MockHostServices>(0, controller_manager);

    controller.AddDevice(device);

    controller.InitCmd(6);

    EXPECT_CALL(controller, Status()).Times(1);
    EXPECT_TRUE(device->Dispatch(scsi_command::eCmdTestUnitReady));
}

TEST(HostServicesTest, StartStopUnit)
{
	MockBus bus;
	ControllerManager controller_manager(bus);
    MockAbstractController controller(0);
	auto device = make_shared<MockHostServices>(0, controller_manager);

    controller.AddDevice(device);

    vector<int>& cmd = controller.InitCmd(6);

    // STOP
    EXPECT_CALL(controller, ScheduleShutdown(AbstractController::rascsi_shutdown_mode::STOP_RASCSI)).Times(1);
    EXPECT_CALL(controller, Status()).Times(1);
    EXPECT_TRUE(device->Dispatch(scsi_command::eCmdStartStop));

    // LOAD
    cmd[4] = 0x02;
    EXPECT_CALL(controller, ScheduleShutdown(AbstractController::rascsi_shutdown_mode::STOP_PI)).Times(1);
    EXPECT_CALL(controller, Status()).Times(1);
    EXPECT_TRUE(device->Dispatch(scsi_command::eCmdStartStop));

    // UNLOAD
    cmd[4] = 0x03;
    EXPECT_CALL(controller, ScheduleShutdown(AbstractController::rascsi_shutdown_mode::RESTART_PI)).Times(1);
    EXPECT_CALL(controller, Status()).Times(1);
    EXPECT_TRUE(device->Dispatch(scsi_command::eCmdStartStop));

    // START
    cmd[4] = 0x01;
    EXPECT_THROW(device->Dispatch(scsi_command::eCmdStartStop), scsi_error_exception);
}

TEST(HostServicesTest, ModeSense6)
{
	MockBus bus;
	ControllerManager controller_manager(bus);
    MockAbstractController controller(0);
	auto device = make_shared<MockHostServices>(1, controller_manager);

    controller.AddDevice(device);

    vector<int>& cmd = controller.InitCmd(6);

    EXPECT_THROW(device->Dispatch(scsi_command::eCmdModeSense6), scsi_error_exception)
    	<< "Unsupported mode page was returned";

    cmd[2] = 0x20;
    EXPECT_THROW(device->Dispatch(scsi_command::eCmdModeSense6), scsi_error_exception)
    	<< "Block descriptors are not supported";

    cmd[1] = 0x08;
    // ALLOCATION LENGTH
    cmd[4] = 255;
    EXPECT_CALL(controller, DataIn()).Times(1);
    EXPECT_TRUE(device->Dispatch(scsi_command::eCmdModeSense6));
	const vector<BYTE>& buffer = controller.GetBuffer();
	// Major version 1
	EXPECT_EQ(0x01, buffer[6]);
	// Minor version 0
	EXPECT_EQ(0x00, buffer[7]);
	// Year
	EXPECT_NE(0x00, buffer[9]);
	// Day
	EXPECT_NE(0x00, buffer[10]);
}

TEST(HostServicesTest, ModeSense10)
{
	MockBus bus;
	ControllerManager controller_manager(bus);
    MockAbstractController controller(0);
	auto device = make_shared<MockHostServices>(1, controller_manager);

    controller.AddDevice(device);

    vector<int>& cmd = controller.InitCmd(10);

    EXPECT_THROW(device->Dispatch(scsi_command::eCmdModeSense10), scsi_error_exception)
    	<< "Unsupported mode page was returned";

    cmd[2] = 0x20;
    EXPECT_THROW(device->Dispatch(scsi_command::eCmdModeSense10), scsi_error_exception)
    	<< "Block descriptors are not supported";

    cmd[1] = 0x08;
    // ALLOCATION LENGTH
    cmd[8] = 255;
    EXPECT_CALL(controller, DataIn()).Times(1);
    EXPECT_TRUE(device->Dispatch(scsi_command::eCmdModeSense10));
	const vector<BYTE>& buffer = controller.GetBuffer();
	// Major version 1
	EXPECT_EQ(0x01, buffer[10]);
	// Minor version 0
	EXPECT_EQ(0x00, buffer[11]);
	// Year
	EXPECT_NE(0x00, buffer[13]);
	// Day
	EXPECT_NE(0x00, buffer[14]);
}

TEST(HostServicesTest, SetUpModePages)
{
	MockBus bus;
	ControllerManager controller_manager(bus);
	MockHostServices device(0, controller_manager);
	map<int, vector<byte>> mode_pages;

	device.SetUpModePages(mode_pages, 0x3f, false);
	EXPECT_EQ(1, mode_pages.size()) << "Unexpected number of mode pages";
	EXPECT_EQ(10, mode_pages[32].size());
}

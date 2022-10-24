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

TEST(HostServicesTest, Dispatch)
{
	TestDispatch(SCHS);
}

TEST(HostServicesTest, TestUnitReady)
{
	NiceMock<MockAbstractController> controller(make_shared<MockBus>(), 0);
	auto services = CreateDevice(SCHS, controller);

    EXPECT_CALL(controller, Status());
    EXPECT_TRUE(services->Dispatch(scsi_command::eCmdTestUnitReady)) << "TEST UNIT READY must never fail";
    EXPECT_EQ(status::GOOD, controller.GetStatus());
}

TEST(HostServicesTest, Inquiry)
{
	TestInquiry(SCHS, device_type::PROCESSOR, scsi_level::SPC_3, "RaSCSI  Host Services   ", 0x1f, false);
}

TEST(HostServicesTest, StartStopUnit)
{
	NiceMock<MockAbstractController> controller(make_shared<MockBus>(), 0);
	auto services = CreateDevice(SCHS, controller);

    vector<int>& cmd = controller.GetCmd();

    // STOP
    EXPECT_CALL(controller, ScheduleShutdown(AbstractController::rascsi_shutdown_mode::STOP_RASCSI));
    EXPECT_CALL(controller, Status());
    EXPECT_TRUE(services->Dispatch(scsi_command::eCmdStartStop));
    EXPECT_EQ(status::GOOD, controller.GetStatus());

    // LOAD
    cmd[4] = 0x02;
    EXPECT_CALL(controller, ScheduleShutdown(AbstractController::rascsi_shutdown_mode::STOP_PI));
    EXPECT_CALL(controller, Status());
    EXPECT_TRUE(services->Dispatch(scsi_command::eCmdStartStop));
    EXPECT_EQ(status::GOOD, controller.GetStatus());

    // UNLOAD
    cmd[4] = 0x03;
    EXPECT_CALL(controller, ScheduleShutdown(AbstractController::rascsi_shutdown_mode::RESTART_PI));
    EXPECT_CALL(controller, Status());
    EXPECT_TRUE(services->Dispatch(scsi_command::eCmdStartStop));
    EXPECT_EQ(status::GOOD, controller.GetStatus());

    // START
    cmd[4] = 0x01;
    EXPECT_THROW(services->Dispatch(scsi_command::eCmdStartStop), scsi_exception);
}

TEST(HostServicesTest, ModeSense6)
{
	NiceMock<MockAbstractController> controller(make_shared<MockBus>(), 0);
	auto services = CreateDevice(SCHS, controller);

    vector<int>& cmd = controller.GetCmd();

    EXPECT_THROW(services->Dispatch(scsi_command::eCmdModeSense6), scsi_exception)
    	<< "Unsupported mode page was returned";

    cmd[2] = 0x20;
    EXPECT_THROW(services->Dispatch(scsi_command::eCmdModeSense6), scsi_exception)
    	<< "Block descriptors are not supported";

    cmd[1] = 0x08;
    // ALLOCATION LENGTH
    cmd[4] = 255;
    EXPECT_CALL(controller, DataIn());
    EXPECT_TRUE(services->Dispatch(scsi_command::eCmdModeSense6));
	vector<BYTE>& buffer = controller.GetBuffer();
	// Major version 1
	EXPECT_EQ(0x01, buffer[6]);
	// Minor version 0
	EXPECT_EQ(0x00, buffer[7]);
	// Year
	EXPECT_NE(0x00, buffer[9]);
	// Day
	EXPECT_NE(0x00, buffer[10]);

    // ALLOCATION LENGTH
    cmd[4] = 2;
    EXPECT_CALL(controller, DataIn());
    EXPECT_TRUE(services->Dispatch(scsi_command::eCmdModeSense6));
	buffer = controller.GetBuffer();
	EXPECT_EQ(0x02, buffer[0]);
}

TEST(HostServicesTest, ModeSense10)
{
	NiceMock<MockAbstractController> controller(make_shared<MockBus>(), 0);
	auto services = CreateDevice(SCHS, controller);

    vector<int>& cmd = controller.GetCmd();

    EXPECT_THROW(services->Dispatch(scsi_command::eCmdModeSense10), scsi_exception)
    	<< "Unsupported mode page was returned";

    cmd[2] = 0x20;
    EXPECT_THROW(services->Dispatch(scsi_command::eCmdModeSense10), scsi_exception)
    	<< "Block descriptors are not supported";

    cmd[1] = 0x08;
    // ALLOCATION LENGTH
    cmd[8] = 255;
    EXPECT_CALL(controller, DataIn());
    EXPECT_TRUE(services->Dispatch(scsi_command::eCmdModeSense10));
	vector<BYTE>& buffer = controller.GetBuffer();
	// Major version 1
	EXPECT_EQ(0x01, buffer[10]);
	// Minor version 0
	EXPECT_EQ(0x00, buffer[11]);
	// Year
	EXPECT_NE(0x00, buffer[13]);
	// Day
	EXPECT_NE(0x00, buffer[14]);

    // ALLOCATION LENGTH
    cmd[8] = 2;
    EXPECT_CALL(controller, DataIn());
    EXPECT_TRUE(services->Dispatch(scsi_command::eCmdModeSense10));
	buffer = controller.GetBuffer();
	EXPECT_EQ(0x02, buffer[1]);
}

TEST(HostServicesTest, SetUpModePages)
{
	ControllerManager controller_manager(make_shared<MockBus>());
	MockHostServices services(0, controller_manager);
	map<int, vector<byte>> pages;

	services.SetUpModePages(pages, 0x3f, false);
	EXPECT_EQ(1, pages.size()) << "Unexpected number of mode pages";
	EXPECT_EQ(10, pages[32].size());
}

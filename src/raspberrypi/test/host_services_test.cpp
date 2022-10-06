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

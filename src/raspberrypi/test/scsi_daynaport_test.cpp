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
#include "devices/scsi_daynaport.h"

TEST(ScsiDaynaportTest, Inquiry)
{
	TestInquiry(SCDP, device_type::PROCESSOR, scsi_level::SCSI_2, scsi_level::SCSI_2,
			"Dayna   SCSI/Link       1.4a", 0x20, false);
}

TEST(ScsiDaynaportTest, Dispatch)
{
	NiceMock<MockAbstractController> controller(0);
	auto daynaport = CreateDevice(SCDP, controller);

	EXPECT_FALSE(daynaport->Dispatch(scsi_command::eCmdModeSense6))
		<< "Non-DaynaPort commands inherited from Disk must not be supported";
	EXPECT_FALSE(daynaport->Dispatch(scsi_command::eCmdModeSelect6))
		<< "Non-DaynaPort commands inherited from Disk must not be supported";
	EXPECT_FALSE(daynaport->Dispatch(scsi_command::eCmdModeSense10))
		<< "Non-DaynaPort commands inherited from Disk must not be supported";
	EXPECT_FALSE(daynaport->Dispatch(scsi_command::eCmdModeSelect10))
		<< "Non-DaynaPort commands inherited from Disk must not be supported";
}

TEST(ScsiDaynaportTest, TestUnitReady)
{
	NiceMock<MockAbstractController> controller(0);
	auto daynaport = CreateDevice(SCDP, controller);

    EXPECT_CALL(controller, Status());
    EXPECT_TRUE(daynaport->Dispatch(scsi_command::eCmdTestUnitReady)) << "TEST UNIT READY must never fail";
    EXPECT_EQ(status::GOOD, controller.GetStatus());
}

TEST(ScsiDaynaportTest, TestRetrieveStats)
{
	NiceMock<MockAbstractController> controller(0);
	auto daynaport = CreateDevice(SCDP, controller);

	vector<int>& cmd = controller.GetCmd();

    // ALLOCATION LENGTH
    cmd[4] = 255;
    EXPECT_CALL(controller, DataIn());
	EXPECT_TRUE(daynaport->Dispatch(scsi_command::eCmdRetrieveStats));
}

TEST(ScsiDaynaportTest, SetInterfaceMode)
{
	NiceMock<MockAbstractController> controller(0);
	auto daynaport = CreateDevice(SCDP, controller);

	vector<int>& cmd = controller.GetCmd();

	// Unknown interface command
	EXPECT_THROW(daynaport->Dispatch(scsi_command::eCmdSetIfaceMode), scsi_exception);

	// Not implemented, do nothing
	cmd[5] = SCSIDaynaPort::CMD_SCSILINK_SETMODE;
	EXPECT_CALL(controller, Status());
	EXPECT_TRUE(daynaport->Dispatch(scsi_command::eCmdSetIfaceMode));
	EXPECT_EQ(status::GOOD, controller.GetStatus());

	cmd[5] = SCSIDaynaPort::CMD_SCSILINK_SETMAC;
	EXPECT_CALL(controller, DataOut());
	EXPECT_TRUE(daynaport->Dispatch(scsi_command::eCmdSetIfaceMode));

	// Not implemented
	cmd[5] = SCSIDaynaPort::CMD_SCSILINK_STATS;
	EXPECT_THROW(daynaport->Dispatch(scsi_command::eCmdSetIfaceMode), scsi_exception);

	// Not implemented
	cmd[5] = SCSIDaynaPort::CMD_SCSILINK_ENABLE;
	EXPECT_THROW(daynaport->Dispatch(scsi_command::eCmdSetIfaceMode), scsi_exception);

	// Not implemented
	cmd[5] = SCSIDaynaPort::CMD_SCSILINK_SET;
	EXPECT_THROW(daynaport->Dispatch(scsi_command::eCmdSetIfaceMode), scsi_exception);
}

TEST(ScsiDaynaportTest, SetMcastAddr)
{
	NiceMock<MockAbstractController> controller(0);
	auto daynaport = CreateDevice(SCDP, controller);

	vector<int>& cmd = controller.GetCmd();

	EXPECT_THROW(daynaport->Dispatch(scsi_command::eCmdSetMcastAddr), scsi_exception) << "Length of 0 is not supported";

	cmd[4] = 1;
    EXPECT_CALL(controller, DataOut());
	EXPECT_TRUE(daynaport->Dispatch(scsi_command::eCmdSetMcastAddr));
}

TEST(ScsiDaynaportTest, EnableInterface)
{
	NiceMock<MockAbstractController> controller(0);
	auto daynaport = CreateDevice(SCDP, controller);

	vector<int>& cmd = controller.GetCmd();

	// Enable
	EXPECT_THROW(daynaport->Dispatch(scsi_command::eCmdEnableInterface), scsi_exception);

	// Disable
	cmd[5] = 0x80;
	EXPECT_THROW(daynaport->Dispatch(scsi_command::eCmdEnableInterface), scsi_exception);
}

TEST(ScsiDaynaportTest, GetSendDelay)
{
	SCSIDaynaPort daynaport(0);

	EXPECT_EQ(6, daynaport.GetSendDelay());
}

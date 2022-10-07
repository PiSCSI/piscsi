//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "devices/scsi_daynaport.h"

TEST(ScsiDaynaportTest, Inquiry)
{
	TestInquiry(SCDP, device_type::PROCESSOR, scsi_level::SCSI_2, scsi_level::SCSI_2,
			"Dayna   SCSI/Link       1.4a", 0x20, false);
}

TEST(ScsiDaynaportTest, Dispatch)
{
	NiceMock<MockAbstractController> controller(0);
	auto daynaport = make_shared<SCSIDaynaPort>(0);

	controller.AddDevice(daynaport);

	controller.InitCmd(10);

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
	auto daynaport = make_shared<SCSIDaynaPort>(0);

    controller.AddDevice(daynaport);

    controller.InitCmd(6);

    EXPECT_CALL(controller, Status()).Times(1);
    EXPECT_TRUE(daynaport->Dispatch(scsi_command::eCmdTestUnitReady)) << "TEST UNIT READY must never fail";
    EXPECT_EQ(status::GOOD, controller.GetStatus());
}

TEST(ScsiDaynaportTest, RetrieveStatistics)
{
	NiceMock<MockAbstractController> controller(0);
	auto daynaport = make_shared<SCSIDaynaPort>(0);

	controller.AddDevice(daynaport);

	vector<int>& cmd = controller.InitCmd(6);

    // ALLOCATION LENGTH
    cmd[4] = 255;
    EXPECT_CALL(controller, DataIn()).Times(1);
	EXPECT_TRUE(daynaport->Dispatch(scsi_command::eCmdRetrieveStats));
}

TEST(ScsiDaynaportTest, GetSendDelay)
{
	SCSIDaynaPort daynaport(0);

	EXPECT_EQ(6, daynaport.GetSendDelay());
}

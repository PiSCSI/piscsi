//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "testing.h"
#include "controllers/scsi_controller.h"

TEST(ScsiControllerTest, GetMaxLuns)
{
	MockScsiController controller(0);

	EXPECT_EQ(32, controller.GetMaxLuns());
}

TEST(ScsiControllerTest, RequestSense)
{
	MockScsiController controller(0);
	MockPrimaryDevice device;

	controller.AddDevice(&device);

	vector<int>& cmd = controller.InitCmd(6);
	// ALLOCATION LENGTH
	cmd[4] = 255;
	// Non-existing LUN
	cmd[1] = 0x20;

	device.SetReady(true);
	EXPECT_CALL(controller, Error(sense_key::ILLEGAL_REQUEST, asc::INVALID_LUN, status::CHECK_CONDITION)).Times(1);
	EXPECT_CALL(controller, DataIn()).Times(1);
	EXPECT_TRUE(device.Dispatch(scsi_command::eCmdRequestSense));
	EXPECT_EQ(0, controller.GetStatus()) << "Illegal CHECK CONDITION for non-exsting LUN";
}

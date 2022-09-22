//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "testing.h"
#include "controllers/abstract_controller.h"

TEST(AbstractControllerTest, DeviceLunLifeCycle)
{
	const int ID = 1;
	const int LUN = 4;

	MockAbstractController controller(ID);
	MockPrimaryDevice device;

	device.SetLun(LUN);

	EXPECT_EQ(ID, controller.GetTargetId());
	EXPECT_TRUE(controller.AddDevice(&device));
	EXPECT_TRUE(controller.HasDeviceForLun(LUN));
	EXPECT_FALSE(controller.HasDeviceForLun(0));
	EXPECT_EQ(&device, controller.GetDeviceForLun(LUN));
	EXPECT_EQ(nullptr, controller.GetDeviceForLun(0));
	EXPECT_TRUE(controller.DeleteDevice(&device));
}

TEST(AbstractControllerTest, ExtractInitiatorId)
{
	const int ID = 1;
	const int INITIATOR_ID = 7;
	const int UNKNOWN_INITIATOR_ID = -1;

	MockAbstractController controller(ID);

	EXPECT_EQ(INITIATOR_ID, controller.ExtractInitiatorId((1 << INITIATOR_ID) | ( 1 << ID)));
	EXPECT_EQ(UNKNOWN_INITIATOR_ID, controller.ExtractInitiatorId(1 << ID));
}

TEST(AbstractControllerTest, GetOpcode)
{
	MockAbstractController controller(0);

	controller.ctrl.cmd.resize(1);

	controller.ctrl.cmd[0] = 0x12;
	EXPECT_EQ(0x12, (int)controller.GetOpcode());
}

TEST(AbstractControllerTest, GetLun)
{
	const int LUN = 3;

	MockAbstractController controller(0);

	controller.ctrl.cmd.resize(2);

	controller.ctrl.cmd[1] = LUN << 5;
	EXPECT_EQ(LUN, controller.GetLun());
}

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

TEST(AbstractControllerTest, AbstractController)
{
	const int ID = 1;
	const int LUN = 4;
	const int INITIATOR_ID = 7;
	const int UNKNOWN_INITIATOR_ID = -1;

	MockAbstractController controller(ID);
	MockPrimaryDevice device;

	device.SetLun(LUN);

	EXPECT_TRUE(controller.AddDevice(&device));
	EXPECT_EQ(ID, controller.GetTargetId());
	EXPECT_TRUE(controller.HasDeviceForLun(LUN));
	EXPECT_FALSE(controller.HasDeviceForLun(0));

	controller.ClearLuns();
	EXPECT_FALSE(controller.HasDeviceForLun(LUN));

	EXPECT_EQ(INITIATOR_ID, controller.ExtractInitiatorId((1 << INITIATOR_ID) | ( 1 << ID)));
	EXPECT_EQ(UNKNOWN_INITIATOR_ID, controller.ExtractInitiatorId(1 << ID));
}

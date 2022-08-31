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

	// TODO Mock abstract controller
	MockScsiController controller(ID);
	MockPrimaryDevice device;

	device.SetId(ID);
	device.SetLun(LUN);

	EXPECT_TRUE(controller.AddDevice(&device));
	EXPECT_EQ(ID, controller.GetTargetId());
	EXPECT_TRUE(controller.HasDeviceForLun(LUN));
	EXPECT_FALSE(controller.HasDeviceForLun(0));
	controller.ClearLuns();
	EXPECT_FALSE(controller.HasDeviceForLun(LUN));

	EXPECT_EQ(7, controller.ExtractInitiatorId((1 << 7) | ( 1 << ID)));
}

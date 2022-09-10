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

TEST(ScsiControllerTest, ScsiController)
{
	MockScsiController controller(nullptr, 0);

	EXPECT_EQ(32, controller.GetMaxLuns());

	EXPECT_CALL(controller, SetPhase(BUS::phase_t::busfree)).Times(1);
	controller.Reset();
}

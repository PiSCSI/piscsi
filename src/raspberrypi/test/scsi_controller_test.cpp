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
	const int ID = 1;
	const int LUN = 4;

	MockBus bus;
	MockScsiController controller(&bus, ID);
	MockPrimaryDevice device;

	device.SetLun(LUN);

	EXPECT_CALL(controller, SetPhase(BUS::phase_t::busfree)).Times(1);
	controller.Reset();
}

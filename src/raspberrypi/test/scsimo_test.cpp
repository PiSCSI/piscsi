//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"

TEST(ScsiMoTest, Inquiry)
{
	TestInquiry(SCMO, device_type::OPTICAL_MEMORY, scsi_level::SCSI_2, scsi_level::SCSI_2,
			"RaSCSI  SCSI MO         ", 0x1f, true);
}

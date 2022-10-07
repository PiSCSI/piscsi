//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"

TEST(ScsiCdTest, Inquiry)
{
	TestInquiry(SCCD, device_type::CD_ROM, scsi_level::SCSI_2, scsi_level::SCSI_2,
			"RaSCSI  SCSI CD-ROM     ", 0x1f, true);
}

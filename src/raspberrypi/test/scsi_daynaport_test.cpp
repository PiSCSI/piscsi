//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"

TEST(ScsiDaynaportTest, Inquiry)
{
	TestInquiry(SCDP, device_type::PROCESSOR, scsi_level::SCSI_2, scsi_level::SCSI_2,
			"Dayna   SCSI/Link       1.4a", 0x20, false);
}

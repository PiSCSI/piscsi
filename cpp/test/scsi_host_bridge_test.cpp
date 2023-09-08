//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"

TEST(ScsiHostBridgeTest, Inquiry)
{
	TestInquiry::Inquiry(SCBR, device_type::communications, scsi_level::scsi_2, "PiSCSI  RASCSI BRIDGE   ", 0x27, false);
}

//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"

TEST(ScsiHostBridgeTest, GetDefaultParams)
{
	const auto [controller, bridge] = CreateDevice(SCBR);
	const auto params = bridge->GetDefaultParams();
	EXPECT_EQ(2, params.size());
}

TEST(ScsiHostBridgeTest, Inquiry)
{
	TestInquiry::Inquiry(SCBR, device_type::communications, scsi_level::scsi_2, "PiSCSI  RASCSI BRIDGE   ", 0x27, false);
}

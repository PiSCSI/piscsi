//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "devices/ctapdriver.h"

TEST(CTapDriver, Crc32)
{
	BYTE buf[ETH_FRAME_LEN];

	memset(buf, 0x00, sizeof(buf));
	EXPECT_EQ(0xe3d887bb, CTapDriver::Crc32(buf, ETH_FRAME_LEN));

	memset(buf, 0xff, sizeof(buf));
	EXPECT_EQ(0x814765f4, CTapDriver::Crc32(buf, ETH_FRAME_LEN));

	memset(buf, 0x10, sizeof(buf));
	EXPECT_EQ(0xb7288Cd3, CTapDriver::Crc32(buf, ETH_FRAME_LEN));

	memset(buf, 0x7f, sizeof(buf));
	EXPECT_EQ(0x4b543477, CTapDriver::Crc32(buf, ETH_FRAME_LEN));

	memset(buf, 0x80, sizeof(buf));
	EXPECT_EQ(0x29cbd638, CTapDriver::Crc32(buf, ETH_FRAME_LEN));

	for (size_t i = 0; i < sizeof(buf); i++) {
		buf[i] = i;
	}
	EXPECT_EQ(0xe7870705, CTapDriver::Crc32(buf, ETH_FRAME_LEN));

	for (size_t i = sizeof(buf) - 1; i > 0; i--) {
		buf[i] = i;
	}
	EXPECT_EQ(0xe7870705, CTapDriver::Crc32(buf, ETH_FRAME_LEN));
}

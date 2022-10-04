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

TEST(CTapDriverTest, Crc32)
{
	array<BYTE, ETH_FRAME_LEN> buf;

	buf.fill(0x00);
	EXPECT_EQ(0xe3d887bb, CTapDriver::Crc32(buf.data(), ETH_FRAME_LEN));

	buf.fill(0xff);
	EXPECT_EQ(0x814765f4, CTapDriver::Crc32(buf.data(), ETH_FRAME_LEN));

	buf.fill(0x10);
	EXPECT_EQ(0xb7288Cd3, CTapDriver::Crc32(buf.data(), ETH_FRAME_LEN));

	buf.fill(0x7f);
	EXPECT_EQ(0x4b543477, CTapDriver::Crc32(buf.data(), ETH_FRAME_LEN));

	buf.fill(0x80);
	EXPECT_EQ(0x29cbd638, CTapDriver::Crc32(buf.data(), ETH_FRAME_LEN));

	for (size_t i = 0; i < buf.size(); i++) {
		buf[i] = (BYTE)i;
	}
	EXPECT_EQ(0xe7870705, CTapDriver::Crc32(buf.data(), ETH_FRAME_LEN));

	for (size_t i = buf.size() - 1; i > 0; i--) {
		buf[i] = (BYTE)i;
	}
	EXPECT_EQ(0xe7870705, CTapDriver::Crc32(buf.data(), ETH_FRAME_LEN));
}

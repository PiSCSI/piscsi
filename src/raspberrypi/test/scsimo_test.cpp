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

TEST(ScsiMoTest, SetUpModePages)
{
	map<int, vector<byte>> mode_pages;
	const unordered_set<uint32_t> sector_sizes;
	MockSCSIMO device(0, sector_sizes);

	device.SetReady(true);
	device.SetUpModePages(mode_pages, 0x3f, false);
	EXPECT_EQ(6, mode_pages.size()) << "Unexpected number of mode pages";
	EXPECT_EQ(12, mode_pages[1].size());
	EXPECT_EQ(24, mode_pages[3].size());
	EXPECT_EQ(24, mode_pages[4].size());
	EXPECT_EQ(4, mode_pages[6].size());
	EXPECT_EQ(12, mode_pages[8].size());
	EXPECT_EQ(12, mode_pages[32].size());
}

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
	TestInquiry(SCCD, device_type::CD_ROM, scsi_level::SCSI_2, "RaSCSI  SCSI CD-ROM     ", 0x1f, true);
}

TEST(ScsiCdTest, Dispatch)
{
	TestDispatch(SCCD);
}

TEST(ScsiCdTest, SetUpModePages)
{
	map<int, vector<byte>> pages;
	const unordered_set<uint32_t> sector_sizes;
	MockSCSICD cd(0, sector_sizes);

	cd.SetUpModePages(pages, 0x3f, false);
	EXPECT_EQ(7, pages.size()) << "Unexpected number of mode pages";
	EXPECT_EQ(12, pages[1].size());
	EXPECT_EQ(24, pages[3].size());
	EXPECT_EQ(24, pages[4].size());
	EXPECT_EQ(12, pages[8].size());
	EXPECT_EQ(8, pages[13].size());
	EXPECT_EQ(16, pages[14].size());
	EXPECT_EQ(30, pages[48].size());
}

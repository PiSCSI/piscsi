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
	MockSCSIMO mo(0, sector_sizes);

	mo.SetReady(false);
	mo.SetUpModePages(mode_pages, 0x3f, false);
	EXPECT_EQ(6, mode_pages.size()) << "Unexpected number of mode pages";
	EXPECT_EQ(12, mode_pages[1].size());
	EXPECT_EQ(24, mode_pages[3].size());
	EXPECT_EQ(24, mode_pages[4].size());
	EXPECT_EQ(4, mode_pages[6].size());
	EXPECT_EQ(12, mode_pages[8].size());
	EXPECT_EQ(12, mode_pages[32].size());
}

TEST(ScsiMoTest, TestAddVendorPage)
{
	map<int, vector<byte>> mode_pages;
	const unordered_set<uint32_t> sector_sizes;
	MockSCSIMO mo(0, sector_sizes);

	mo.SetReady(true);
	mo.SetBlockCount(0x12345678);
	mo.SetUpModePages(mode_pages, 0x20, false);
	EXPECT_EQ(1, mode_pages.size()) << "Unexpected number of mode pages";
	vector<byte>& mode_page_32 = mode_pages[32];
	EXPECT_EQ(12, mode_page_32.size());
	EXPECT_EQ(0, (int)mode_page_32[2])  << "Wrong format mode";
	EXPECT_EQ(0, (int)mode_page_32[3])  << "Wrong format type";
	EXPECT_EQ(0x12, (int)mode_page_32[4]);
	EXPECT_EQ(0x34, (int)mode_page_32[5]);
	EXPECT_EQ(0x56, (int)mode_page_32[6]);
	EXPECT_EQ(0x78, (int)mode_page_32[7]);
	EXPECT_EQ(0x00, (int)mode_page_32[8]);
	EXPECT_EQ(0x00, (int)mode_page_32[9]);
	EXPECT_EQ(0x00, (int)mode_page_32[10]);
	EXPECT_EQ(0x00, (int)mode_page_32[11]);

	mo.SetBlockCount(248826);
	mo.SetSectorSizeInBytes(512);
	mo.SetUpModePages(mode_pages, 0x20, false);
	EXPECT_EQ(1, mode_pages.size()) << "Unexpected number of mode pages";
	mode_page_32 = mode_pages[32];
	EXPECT_EQ(12, mode_page_32.size());
	EXPECT_EQ(0, (int)mode_page_32[2])  << "Wrong format mode";
	EXPECT_EQ(0, (int)mode_page_32[3])  << "Wrong format type";
	EXPECT_EQ(0x04, (int)mode_page_32[8]);
	EXPECT_EQ(0x00, (int)mode_page_32[9]);
	EXPECT_EQ(0x00, (int)mode_page_32[10]);
	EXPECT_EQ(0x01, (int)mode_page_32[11]);
}

//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"

void ScsiMo_SetUpModePages(map<int, vector<byte>>& pages)
{
	EXPECT_EQ(6, pages.size()) << "Unexpected number of mode pages";
	EXPECT_EQ(12, pages[1].size());
	EXPECT_EQ(24, pages[3].size());
	EXPECT_EQ(24, pages[4].size());
	EXPECT_EQ(4, pages[6].size());
	EXPECT_EQ(12, pages[8].size());
	EXPECT_EQ(12, pages[32].size());
}

TEST(ScsiMoTest, Inquiry)
{
	TestInquiry(SCMO, device_type::OPTICAL_MEMORY, scsi_level::SCSI_2, "RaSCSI  SCSI MO         ", 0x1f, true);
}

TEST(ScsiMoTest, SupportsSaveParameters)
{
	map<int, vector<byte>> pages;
	const unordered_set<uint32_t> sector_sizes;
	MockSCSIMO mo(0, sector_sizes);

	EXPECT_TRUE(mo.SupportsSaveParameters());
}

TEST(ScsiMoTest, SetUpModePages)
{
	map<int, vector<byte>> pages;
	const unordered_set<uint32_t> sector_sizes;
	MockSCSIMO mo(0, sector_sizes);

	// Non changeable
	mo.SetUpModePages(pages, 0x3f, false);
	ScsiMo_SetUpModePages(pages);

	// Changeable
	pages.clear();
	mo.SetUpModePages(pages, 0x3f, true);
	ScsiMo_SetUpModePages(pages);
}

TEST(ScsiMoTest, TestAddVendorPage)
{
	map<int, vector<byte>> pages;
	const unordered_set<uint32_t> sector_sizes;
	MockSCSIMO mo(0, sector_sizes);

	mo.SetReady(true);
	mo.SetUpModePages(pages, 0x21, false);
	EXPECT_TRUE(pages.empty()) << "Unsupported vendor-specific page was returned";

	mo.SetBlockCount(0x12345678);
	mo.SetUpModePages(pages, 0x20, false);
	EXPECT_EQ(1, pages.size()) << "Unexpected number of mode pages";
	vector<byte>& page_32 = pages[32];
	EXPECT_EQ(12, page_32.size());
	EXPECT_EQ(0, (int)page_32[2])  << "Wrong format mode";
	EXPECT_EQ(0, (int)page_32[3])  << "Wrong format type";
	EXPECT_EQ(0x12345678, GetInt32(page_32, 4)) << "Wrong number of blocks";
	EXPECT_EQ(0, GetInt16(page_32, 8)) << "Wrong number of spare blocks";
	EXPECT_EQ(0, GetInt16(page_32, 10));

	mo.SetSectorSizeInBytes(512);
	mo.SetUpModePages(pages, 0x20, false);
	page_32 = pages[32];
	EXPECT_EQ(0, GetInt16(page_32, 8)) << "Wrong number of spare blocks";
	EXPECT_EQ(0, GetInt16(page_32, 10));

	mo.SetBlockCount(248826);
	mo.SetUpModePages(pages, 0x20, false);
	page_32 = pages[32];
	EXPECT_EQ(1024, GetInt16(page_32, 8)) << "Wrong number of spare blocks";
	EXPECT_EQ(1, GetInt16(page_32, 10));

	mo.SetBlockCount(446325);
	mo.SetUpModePages(pages, 0x20, false);
	page_32 = pages[32];
	EXPECT_EQ(1025, GetInt16(page_32, 8)) << "Wrong number of spare blocks";
	EXPECT_EQ(10, GetInt16(page_32, 10));

	mo.SetBlockCount(1041500);
	mo.SetUpModePages(pages, 0x20, false);
	page_32 = pages[32];
	EXPECT_EQ(2250, GetInt16(page_32, 8)) << "Wrong number of spare blocks";
	EXPECT_EQ(18, GetInt16(page_32, 10));

	mo.SetSectorSizeInBytes(2048);
	mo.SetBlockCount(0x12345678);
	mo.SetUpModePages(pages, 0x20, false);
	page_32 = pages[32];
	EXPECT_EQ(0, GetInt16(page_32, 8)) << "Wrong number of spare blocks";
	EXPECT_EQ(0, GetInt16(page_32, 10));

	mo.SetBlockCount(310352);
	mo.SetUpModePages(pages, 0x20, false);
	page_32 = pages[32];
	EXPECT_EQ(2244, GetInt16(page_32, 8)) << "Wrong number of spare blocks";
	EXPECT_EQ(11, GetInt16(page_32, 10));

	mo.SetBlockCount(605846);
	mo.SetUpModePages(pages, 0x20, false);
	page_32 = pages[32];
	EXPECT_EQ(4437, GetInt16(page_32, 8)) << "Wrong number of spare blocks";
	EXPECT_EQ(18, GetInt16(page_32, 10));

	// Changeable page
	mo.SetUpModePages(pages, 0x20, true);
	EXPECT_EQ(0, (int)page_32[2]);
	EXPECT_EQ(0, (int)page_32[3]);
	EXPECT_EQ(0, GetInt32(page_32, 4));
	EXPECT_EQ(0, GetInt16(page_32, 8));
	EXPECT_EQ(0, GetInt16(page_32, 10));
}

TEST(ScsiMoTest, ModeSelect)
{
	const unordered_set<uint32_t> sector_sizes = { 1024, 2048 };
	MockSCSIMO mo(0, sector_sizes);
	vector<int> cmd(10);
	vector<BYTE> buf(255);

	mo.SetSectorSizeInBytes(2048);

	// PF
	cmd[1] = 0x10;
	// Page 3 (Device Format Page)
	buf[4] = 0x03;
	// 2048 bytes per sector
	buf[16] = 0x08;
	EXPECT_NO_THROW(mo.ModeSelect(scsi_command::eCmdModeSelect6, cmd, buf, 255)) << "MODE SELECT(6) is supported";
	buf[4] = 0;
	buf[16] = 0;

	// Page 3 (Device Format Page)
	buf[8] = 0x03;
	// 2048 bytes per sector
	buf[20] = 0x08;
	EXPECT_NO_THROW(mo.ModeSelect(scsi_command::eCmdModeSelect10, cmd, buf, 255)) << "MODE SELECT(10) is supported";
}

TEST(ScsiMoTest, Dispatch)
{
	TestDispatch(SCMO);
}


//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "controllers/controller_manager.h"
#include "devices/scsihd_nec.h"

using namespace std;

TEST(ScsiHdNecTest, Inquiry)
{
	TestInquiry(SCHD, device_type::DIRECT_ACCESS, scsi_level::SCSI_2, scsi_level::SCSI_2,
			"RaSCSI                  ", 0x1f, false);
}

TEST(ScsiHdNecTest, SetUpModePages)
{
	map<int, vector<byte>> pages;
	MockSCSIHD_NEC hd(0);

	hd.SetUpModePages(pages, 0x3f, false);
	EXPECT_EQ(5, pages.size()) << "Unexpected number of mode pages";
	EXPECT_EQ(8, pages[1].size());
	EXPECT_EQ(24, pages[3].size());
	EXPECT_EQ(20, pages[4].size());
	EXPECT_EQ(12, pages[8].size());
	EXPECT_EQ(30, pages[48].size());
}

TEST(ScsiHdNecTest, TestAddFormatPage)
{
	map<int, vector<byte>> pages;
	MockSCSIHD_NEC hd(0);

	hd.SetBlockCount(0x1234);
	hd.SetReady(true);
	hd.SetUpModePages(pages, 0x03, false);
	EXPECT_EQ(1, pages.size()) << "Unexpected number of mode pages";
	vector<byte>& page_3 = pages[3];
	EXPECT_EQ(0x80, (int)page_3[0] & 0x80);
	EXPECT_EQ(0, (int)page_3[20]);

	hd.SetRemovable(true);
	hd.SetUpModePages(pages, 0x03, false);
	page_3 = pages[3];
	EXPECT_EQ(0x20, (int)page_3[20]);

	hd.SetUpModePages(pages, 0x03, true);
	EXPECT_EQ(0xffff, GetInt16(page_3, 12));
}

TEST(ScsiHdNecTest, TestAddDrivePage)
{
	map<int, vector<byte>> pages;
	MockSCSIHD_NEC hd(0);

	hd.SetBlockCount(0x1234);
	hd.SetReady(true);
	hd.SetUpModePages(pages, 0x04, false);
	EXPECT_EQ(1, pages.size()) << "Unexpected number of mode pages";
}

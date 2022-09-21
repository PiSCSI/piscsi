//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "testing.h"
#include "devices/scsi_command_util.h"

TEST(ScsiCommandUtilTest, EnrichFormatPage)
{
	const int SECTOR_SIZE = 512;

	map<int, vector<byte>> pages;
	vector<byte> format_page(24);
	pages[3] = format_page;

	scsi_command_util::EnrichFormatPage(pages, false, SECTOR_SIZE);
	format_page = pages[3];
	EXPECT_EQ(byte{0}, format_page[12]);
	EXPECT_EQ(byte{0}, format_page[13]);

	scsi_command_util::EnrichFormatPage(pages, true, SECTOR_SIZE);
	format_page = pages[3];
	EXPECT_EQ(byte{SECTOR_SIZE >> 8}, format_page[12]);
	EXPECT_EQ(byte{0}, format_page[13]);
}

TEST(ScsiCommandUtilTest, AddAppleVendorModePage)
{
	map<int, vector<byte>> pages;
	vector<byte> vendor_page(30);
	pages[48] = vendor_page;

	scsi_command_util::AddAppleVendorModePage(pages, true);
	vendor_page = pages[48];
	EXPECT_EQ(byte{0}, vendor_page[2]);

	scsi_command_util::AddAppleVendorModePage(pages, false);
	vendor_page = pages[48];
	EXPECT_STREQ("APPLE COMPUTER, INC   ", (const char *)&vendor_page[2]);
}

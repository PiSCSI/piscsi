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
	map<int, vector<byte>> mode_pages;
	MockSCSIHD_NEC hd(0);

	hd.SetUpModePages(mode_pages, 0x3f, false);
	EXPECT_EQ(5, mode_pages.size()) << "Unexpected number of mode pages";
	EXPECT_EQ(8, mode_pages[1].size());
	EXPECT_EQ(24, mode_pages[3].size());
	EXPECT_EQ(20, mode_pages[4].size());
	EXPECT_EQ(12, mode_pages[8].size());
	EXPECT_EQ(30, mode_pages[48].size());
}


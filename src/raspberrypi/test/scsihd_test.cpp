//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "rascsi_exceptions.h"
#include "devices/scsihd.h"

TEST(ScsiHdTest, FinalizeSetup)
{
	map<int, vector<byte>> mode_pages;
	const unordered_set<uint32_t> sector_sizes;
	MockSCSIHD hd(0, sector_sizes, false);

	hd.SetSectorSizeInBytes(1024);
	EXPECT_THROW(hd.FinalizeSetup(2LL * 1024 * 1024 * 1024 * 1024 + 1, 0), io_exception)
		<< "Unsupported drive capacity";

	EXPECT_THROW(hd.FinalizeSetup(0), io_exception) << "Device has 0 blocks";
}

TEST(ScsiHdTest, Inquiry)
{
	TestInquiry(SCHD, device_type::DIRECT_ACCESS, scsi_level::SCSI_2, scsi_level::SCSI_2,
			"RaSCSI                  ", 0x1f, false);
}

TEST(ScsiHdTest, SetUpModePages)
{
	map<int, vector<byte>> mode_pages;
	const unordered_set<uint32_t> sector_sizes;
	MockSCSIHD hd(0, sector_sizes, false);

	hd.SetReady(false);
	hd.SetUpModePages(mode_pages, 0x3f, false);
	EXPECT_EQ(5, mode_pages.size()) << "Unexpected number of mode pages";
	EXPECT_EQ(12, mode_pages[1].size());
	EXPECT_EQ(24, mode_pages[3].size());
	EXPECT_EQ(24, mode_pages[4].size());
	EXPECT_EQ(12, mode_pages[8].size());
	EXPECT_EQ(30, mode_pages[48].size());
}

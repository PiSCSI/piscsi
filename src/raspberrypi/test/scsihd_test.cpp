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
	EXPECT_THROW(hd.FinalizeSetup(2LL * 1024 * 1024 * 1024 * 1024 + hd.GetSectorSizeInBytes(), 0), io_exception)
		<< "Unsupported drive capacity";

	EXPECT_THROW(hd.FinalizeSetup(0), io_exception) << "Device has 0 blocks";

	hd.SetBlockCount(1);
	hd.FinalizeSetup(2LL * 1024 * 1024 * 1024 * 1024);
	hd.FinalizeSetup(2LL * 1024 * 1024 * 1024 * 1024 + hd.GetSectorSizeInBytes() - 1);
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

TEST(ScsiHdTest, ModeSelect)
{
	map<int, vector<byte>> mode_pages;
	const unordered_set<uint32_t> sector_sizes = { 512 };
	MockSCSIHD hd(0, sector_sizes, false);
	vector<int> cmd(10);
	vector<BYTE> buf(255);

	hd.SetSectorSizeInBytes(512);

	// PF
	cmd[1] = 0x10;
	// Page 3 (Device Format Page)
	buf[4] = 0x03;
	// 512 bytes per sector
	buf[16] = 0x02;
	EXPECT_NO_THROW(hd.ModeSelect(scsi_command::eCmdModeSelect6, cmd, buf, 255)) << "MODE SELECT(6) is supported";
	buf[4] = 0;
	buf[16] = 0;

	// Page 3 (Device Format Page)
	buf[8] = 0x03;
	// 512 bytes per sector
	buf[20] = 0x02;
	EXPECT_NO_THROW(hd.ModeSelect(scsi_command::eCmdModeSelect10, cmd, buf, 255)) << "MODE SELECT(10) is supported";
}

TEST(ScsiHdTest, Inquiry)
{
	TestInquiry(SCHD, device_type::DIRECT_ACCESS, scsi_level::SCSI_2, scsi_level::SCSI_2,
			"RaSCSI                  ", 0x1f, false);
}

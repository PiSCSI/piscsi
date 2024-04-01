//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/piscsi_exceptions.h"
#include "devices/scsihd.h"

void ScsiHdTest_SetUpModePages(map<int, vector<byte>>& pages)
{
	EXPECT_EQ(6, pages.size()) << "Unexpected number of mode pages";
	EXPECT_EQ(12, pages[1].size());
	EXPECT_EQ(24, pages[3].size());
	EXPECT_EQ(24, pages[4].size());
	EXPECT_EQ(12, pages[8].size());
	EXPECT_EQ(25, pages[37].size());
	EXPECT_EQ(30, pages[48].size());
}

TEST(ScsiHdTest, Inquiry)
{
	TestInquiry::Inquiry(SCHD, device_type::direct_access, scsi_level::scsi_2, "PiSCSI                  ", 0x1f, false);

	TestInquiry::Inquiry(SCHD, device_type::direct_access, scsi_level::scsi_1_ccs, "PiSCSI                  ", 0x1f, false, "file.hd1");
}

TEST(ScsiHdTest, SupportsSaveParameters)
{
	MockSCSIHD hd(0, false);

	EXPECT_TRUE(hd.SupportsSaveParameters());
}

TEST(ScsiHdTest, FinalizeSetup)
{
	MockSCSIHD hd(0, false);

	hd.SetSectorSizeInBytes(1024);
	EXPECT_THROW(hd.FinalizeSetup(0), io_exception) << "Device has 0 blocks";
}

TEST(ScsiHdTest, GetProductData)
{
	MockSCSIHD hd_kb(0, false);
	MockSCSIHD hd_mb(0, false);
	MockSCSIHD hd_gb(0, false);

	const path filename = CreateTempFile(1);
	hd_kb.SetFilename(string(filename));
	hd_kb.SetSectorSizeInBytes(1024);
	hd_kb.SetBlockCount(1);
	hd_kb.FinalizeSetup(0);
	string s = hd_kb.GetProduct();
	EXPECT_NE(string::npos, s.find("1 KiB"));

	hd_mb.SetFilename(string(filename));
	hd_mb.SetSectorSizeInBytes(1024);
	hd_mb.SetBlockCount(1'048'576 / 1024);
	hd_mb.FinalizeSetup(0);
	s = hd_mb.GetProduct();
	EXPECT_NE(string::npos, s.find("1 MiB"));

	hd_gb.SetFilename(string(filename));
	hd_gb.SetSectorSizeInBytes(1024);
	hd_gb.SetBlockCount(10'737'418'240 / 1024);
	hd_gb.FinalizeSetup(0);
	s = hd_gb.GetProduct();
	EXPECT_NE(string::npos, s.find("10 GiB"));
	remove(filename);
}

TEST(ScsiHdTest, GetSectorSizes)
{
	MockSCSIHD hd(0, false);

	const auto& sector_sizes = hd.GetSupportedSectorSizes();
	EXPECT_EQ(4, sector_sizes.size());

	EXPECT_TRUE(sector_sizes.contains(512));
	EXPECT_TRUE(sector_sizes.contains(1024));
	EXPECT_TRUE(sector_sizes.contains(2048));
	EXPECT_TRUE(sector_sizes.contains(4096));
}

TEST(ScsiHdTest, SetUpModePages)
{
	map<int, vector<byte>> pages;
	MockSCSIHD hd(0, false);

	// Non changeable
	hd.SetUpModePages(pages, 0x3f, false);
	ScsiHdTest_SetUpModePages(pages);

	// Changeable
	pages.clear();
	hd.SetUpModePages(pages, 0x3f, true);
	ScsiHdTest_SetUpModePages(pages);
}

TEST(ScsiHdTest, DECSpecialFunctionControlPage)
{
	map<int, vector<byte>> pages;
	vector<byte> buf;
	MockSCSIHD hd(0, false);

	EXPECT_NO_THROW(hd.SetUpModePages(pages, 0x25, false)) << "MODE SENSE(6) DEC unique page is supported";
	EXPECT_NE(pages.end(), pages.find(0x25));
	buf = pages[0x25];
	EXPECT_EQ(static_cast<byte> (0x25 | 0x80), buf[0]);
	EXPECT_EQ(static_cast<byte> (0x17), buf[1]);
	EXPECT_EQ(static_cast<byte> (0x01), buf[2]);
}

TEST(ScsiHdTest, ModeSelect)
{
	MockSCSIHD hd({ 512 });
	vector<int> cmd(10);
	vector<uint8_t> buf(255);

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

// Test for the ModeSelect6, page code 1 (issued by Alpha VMS)
TEST(ScsiHdTest, PageCode1)
{
	MockSCSIHD hd(0, false);
	vector<int> cmd = { 0x15, 0x10, 0x00, 0x00, 0x19, 0x00 };
	vector<uint8_t> buf = { 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00
		, 0x01, 0x0a, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };

	EXPECT_NO_THROW(hd.ModeSelect(scsi_command::eCmdModeSelect6, cmd, buf, buf.size())) << "Page code 1 is supported";
}

// Test for the ModeSelect6, multiple pages
TEST(ScsiHdTest, MultiplePages)
{
	MockSCSIHD hd(0, false);
	vector<uint8_t> buf = { 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00
                , 0x02, 0x01, 0x00  // 12
		, 0x02, 0x01, 0x00  // 15
		, 0x02, 0x01, 0x00  // 18
		, 0x02, 0x01, 0x00  // 21
		, 0x02, 0x01, 0x00  // 24
		, 0x02, 0x01, 0x00  // 27
		, 0x02, 0x01, 0x00  // 30
		, 0x02, 0x01, 0x00  // 33
		, 0x02, 0x01, 0x00  // 36
		, 0x02, 0x01, 0x00  // 39
		, 0x02, 0x01, 0x00  // 42
		, 0x02, 0x01, 0x00  // 45
		, 0x02, 0x01, 0x00  // 48
		, 0x02, 0x01, 0x00  // 51
		, 0x02, 0x01, 0x00  // 54
		, 0x02, 0x01, 0x00  // 57
		, 0x02, 0x01, 0x00  // 60
		, 0x02, 0x01, 0x00  // 63
		, 0x02, 0x01, 0x00  // 66
		, 0x02, 0x01, 0x00  // 69
		, 0x02, 0x01, 0x00  // 72
		, 0x02, 0x01, 0x00  // 75
		, 0x02, 0x01, 0x00  // 78
		, 0x02, 0x01, 0x00  // 81
		, 0x02, 0x01, 0x00  // 84
		, 0x02, 0x01, 0x00  // 87
		, 0x02, 0x01, 0x00  // 90
		, 0x03, 0x16, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x19, 0x08, 0x00, 0x00, 0x01, 0x00, 0x0b, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00
        };
	vector<int> cmd = { 0x15, 0x10, 0x00, 0x00, static_cast<int>(buf.size()), 0x00 };

	hd.SetSectorSizeInBytes(2048); // pass the "page 3" sector_size test in scsi_command_util::ModeSelect
	EXPECT_NO_THROW(hd.ModeSelect(scsi_command::eCmdModeSelect6, cmd, buf, buf.size())) << "Multiple pages are supported";
}

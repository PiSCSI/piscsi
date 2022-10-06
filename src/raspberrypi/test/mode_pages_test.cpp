//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "spdlog/spdlog.h"
#include "rascsi_exceptions.h"
#include "controllers/controller_manager.h"
#include "devices/scsi_command_util.h"
#include "devices/scsihd.h"
#include "devices/scsihd_nec.h"
#include "devices/scsicd.h"
#include "devices/scsimo.h"
#include "devices/host_services.h"

using namespace std;

TEST(ModePagesTest, SCSIHD_SetUpModePages)
{
	map<int, vector<byte>> mode_pages;
	const unordered_set<uint32_t> sector_sizes;
	MockSCSIHD device(0, sector_sizes, false);
	device.SetUpModePages(mode_pages, 0x3f, false);

	EXPECT_EQ(5, mode_pages.size()) << "Unexpected number of mode pages";
	EXPECT_EQ(12, mode_pages[1].size());
	EXPECT_EQ(24, mode_pages[3].size());
	EXPECT_EQ(24, mode_pages[4].size());
	EXPECT_EQ(12, mode_pages[8].size());
	EXPECT_EQ(30, mode_pages[48].size());
}

TEST(ModePagesTest, SCSIHD_NEC_SetUpModePages)
{
	map<int, vector<byte>> mode_pages;
	MockSCSIHD_NEC device(0);
	device.SetUpModePages(mode_pages, 0x3f, false);

	EXPECT_EQ(5, mode_pages.size()) << "Unexpected number of mode pages";
	EXPECT_EQ(8, mode_pages[1].size());
	EXPECT_EQ(24, mode_pages[3].size());
	EXPECT_EQ(20, mode_pages[4].size());
	EXPECT_EQ(12, mode_pages[8].size());
	EXPECT_EQ(30, mode_pages[48].size());
}

TEST(ModePagesTest, SCSICD_SetUpModePages)
{
	map<int, vector<byte>> mode_pages;
	const unordered_set<uint32_t> sector_sizes;
	MockSCSICD device(0, sector_sizes);
	device.SetUpModePages(mode_pages, 0x3f, false);

	EXPECT_EQ(7, mode_pages.size()) << "Unexpected number of mode pages";
	EXPECT_EQ(12, mode_pages[1].size());
	EXPECT_EQ(24, mode_pages[3].size());
	EXPECT_EQ(24, mode_pages[4].size());
	EXPECT_EQ(12, mode_pages[8].size());
	EXPECT_EQ(8, mode_pages[13].size());
	EXPECT_EQ(16, mode_pages[14].size());
	EXPECT_EQ(30, mode_pages[48].size());
}

TEST(ModePagesTest, SCSIMO_SetUpModePages)
{
	map<int, vector<byte>> mode_pages;
	const unordered_set<uint32_t> sector_sizes;
	MockSCSIMO device(0, sector_sizes);
	device.SetUpModePages(mode_pages, 0x3f, false);

	EXPECT_EQ(6, mode_pages.size()) << "Unexpected number of mode pages";
	EXPECT_EQ(12, mode_pages[1].size());
	EXPECT_EQ(24, mode_pages[3].size());
	EXPECT_EQ(24, mode_pages[4].size());
	EXPECT_EQ(4, mode_pages[6].size());
	EXPECT_EQ(12, mode_pages[8].size());
	EXPECT_EQ(12, mode_pages[32].size());
}

TEST(ModePagesTest, ModeSelect)
{
	const int LENGTH = 12;

	vector<int> cdb(16);
	vector<BYTE> buf(255);

	// PF (vendor-specific parameter format)
	cdb[1] = 0x00;
	EXPECT_THROW(scsi_command_util::ModeSelect(cdb, buf, LENGTH, 0), scsi_exception)
		<< "Vendor-specific parameters are not supported";

	// PF (standard parameter format)
	cdb[1] = 0x10;
	// Request 512 bytes per sector
	buf[9] = 0x00;
	buf[10] = 0x02;
	buf[11] = 0x00;
	EXPECT_THROW(scsi_command_util::ModeSelect(cdb, buf, LENGTH, 256), scsi_exception)
		<< "Requested sector size does not match current sector size";

	// Page 0
	buf[LENGTH] = 0x00;
	EXPECT_THROW(scsi_command_util::ModeSelect(cdb, buf, LENGTH + 2, 512), scsi_exception)
		<< "Unsupported page 0 was not rejected";

	// Page 3 (Format Device Page)
	buf[LENGTH] = 0x03;
	EXPECT_THROW(scsi_command_util::ModeSelect(cdb, buf, LENGTH + 2, 512), scsi_exception)
		<< "Requested sector size does not match current sector size";

	// Match the requested to the current sector size
	buf[LENGTH + 12] = 0x02;
	scsi_command_util::ModeSelect(cdb, buf, LENGTH + 2, 512);
}

//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "testing.h"
#include "spdlog/spdlog.h"
#include "rascsi_exceptions.h"
#include "devices/scsi_command_util.h"
#include "devices/scsihd.h"
#include "devices/scsihd_nec.h"
#include "devices/scsicd.h"
#include "devices/scsimo.h"
#include "devices/host_services.h"

using namespace std;

TEST(ModePagesTest, ModePageDevice_AddModePages)
{
	vector<int> cdb(6);
	vector<BYTE> buf(512);

	MockModePageDevice device;
	cdb[2] = 0x3f;

	EXPECT_EQ(0, device.AddModePages(cdb, buf.data(), 0))  << "Allocation length was not limited";
	EXPECT_EQ(1, device.AddModePages(cdb, buf.data(), 1))  << "Allocation length was not limited";

	cdb[2] = 0x00;
	EXPECT_THROW(device.AddModePages(cdb, buf.data(), 12), scsi_error_exception) << "Data for non-existing mode page 0 were returned";
}

TEST(ModePagesTest, SCSIHD_AddModePages)
{
	map<int, vector<byte>> mode_pages;
	const unordered_set<uint32_t> sector_sizes;
	MockSCSIHD device(sector_sizes);
	device.AddModePages(mode_pages, 0x3f, false);

	EXPECT_EQ(5, mode_pages.size()) << "Unexpected number of mode pages";
	EXPECT_EQ(12, mode_pages[1].size());
	EXPECT_EQ(24, mode_pages[3].size());
	EXPECT_EQ(24, mode_pages[4].size());
	EXPECT_EQ(12, mode_pages[8].size());
	EXPECT_EQ(30, mode_pages[48].size());
}

TEST(ModePagesTest, SCSIHD_NEC_AddModePages)
{
	map<int, vector<byte>> mode_pages;
	MockSCSIHD_NEC device;
	device.AddModePages(mode_pages, 0x3f, false);

	EXPECT_EQ(5, mode_pages.size()) << "Unexpected number of mode pages";
	EXPECT_EQ(8, mode_pages[1].size());
	EXPECT_EQ(24, mode_pages[3].size());
	EXPECT_EQ(20, mode_pages[4].size());
	EXPECT_EQ(12, mode_pages[8].size());
	EXPECT_EQ(30, mode_pages[48].size());
}

TEST(ModePagesTest, SCSICD_AddModePages)
{
	map<int, vector<byte>> mode_pages;
	const unordered_set<uint32_t> sector_sizes;
	MockSCSICD device(sector_sizes);
	device.AddModePages(mode_pages, 0x3f, false);

	EXPECT_EQ(7, mode_pages.size()) << "Unexpected number of mode pages";
	EXPECT_EQ(12, mode_pages[1].size());
	EXPECT_EQ(24, mode_pages[3].size());
	EXPECT_EQ(24, mode_pages[4].size());
	EXPECT_EQ(12, mode_pages[8].size());
	EXPECT_EQ(8, mode_pages[13].size());
	EXPECT_EQ(16, mode_pages[14].size());
	EXPECT_EQ(30, mode_pages[48].size());
}

TEST(ModePagesTest, SCSIMO_AddModePages)
{
	map<int, vector<byte>> mode_pages;
	unordered_map<uint64_t, Geometry> geometries;
	const unordered_set<uint32_t> sector_sizes;
	MockSCSIMO device(sector_sizes, geometries);
	device.AddModePages(mode_pages, 0x3f, false);

	EXPECT_EQ(6, mode_pages.size()) << "Unexpected number of mode pages";
	EXPECT_EQ(12, mode_pages[1].size());
	EXPECT_EQ(24, mode_pages[3].size());
	EXPECT_EQ(24, mode_pages[4].size());
	EXPECT_EQ(4, mode_pages[6].size());
	EXPECT_EQ(12, mode_pages[8].size());
	EXPECT_EQ(12, mode_pages[32].size());
}

TEST(ModePagesTest, HostServices_AddModePages)
{
	map<int, vector<byte>> mode_pages;
	DeviceFactory device_factory;
	MockHostServices device(device_factory);
	device.AddModePages(mode_pages, 0x3f, false);

	EXPECT_EQ(1, mode_pages.size()) << "Unexpected number of mode pages";
	EXPECT_EQ(10, mode_pages[32].size());
}

TEST(ModePagesTest, ModeSelect)
{
	const int LENGTH = 12;

	vector<int> cdb(16);
	vector<BYTE> buf(255);

	// PF (vendor-specific parameter format)
	cdb[1] = 0x00;
	EXPECT_THROW(scsi_command_util::ModeSelect(cdb, buf.data(), LENGTH, 0), scsi_error_exception)
		<< "Vendor-specific parameters are not supported";

	// PF (standard parameter format)
	cdb[1] = 0x10;
	// Request 512 bytes per sector
	buf[9] = 0x00;
	buf[10] = 0x02;
	buf[11] = 0x00;
	EXPECT_THROW(scsi_command_util::ModeSelect(cdb, buf.data(), LENGTH, 256), scsi_error_exception)
		<< "Requested sector size does not match current sector size";

	// Page 0
	buf[LENGTH] = 0x00;
	EXPECT_THROW(scsi_command_util::ModeSelect(cdb, buf.data(), LENGTH + 2, 512), scsi_error_exception)
		<< "Unsupported page 0 was not rejected";

	// Page 3 (Format Device Page)
	buf[LENGTH] = 0x03;
	EXPECT_THROW(scsi_command_util::ModeSelect(cdb, buf.data(), LENGTH + 2, 512), scsi_error_exception)
		<< "Requested sector size does not match current sector size";

	// Match the requested to the current sector size
	buf[LENGTH + 12] = 0x02;
	scsi_command_util::ModeSelect(cdb, buf.data(), LENGTH + 2, 512);
}

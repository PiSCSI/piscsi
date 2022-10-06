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
#include "devices/scsi_command_util.h"

using namespace scsi_command_util;

TEST(ScsiCommandUtilTest, ModeSelect)
{
	const int LENGTH = 12;

	vector<int> cdb(16);
	vector<BYTE> buf(255);

	// PF (vendor-specific parameter format)
	cdb[1] = 0x00;
	EXPECT_THROW(ModeSelect(cdb, buf, LENGTH, 0), scsi_error_exception)
		<< "Vendor-specific parameters are not supported";

	// PF (standard parameter format)
	cdb[1] = 0x10;
	// Request 512 bytes per sector
	buf[9] = 0x00;
	buf[10] = 0x02;
	buf[11] = 0x00;
	EXPECT_THROW(ModeSelect(cdb, buf, LENGTH, 256), scsi_error_exception)
		<< "Requested sector size does not match current sector size";

	// Page 0
	buf[LENGTH] = 0x00;
	EXPECT_THROW(ModeSelect(cdb, buf, LENGTH + 2, 512), scsi_error_exception)
		<< "Unsupported page 0 was not rejected";

	// Page 3 (Format Device Page)
	buf[LENGTH] = 0x03;
	EXPECT_THROW(ModeSelect(cdb, buf, LENGTH + 2, 512), scsi_error_exception)
		<< "Requested sector size does not match current sector size";

	// Match the requested to the current sector size
	buf[LENGTH + 12] = 0x02;
	ModeSelect(cdb, buf, LENGTH + 2, 512);
}

TEST(ScsiCommandUtilTest, EnrichFormatPage)
{
	const int SECTOR_SIZE = 512;

	map<int, vector<byte>> pages;
	vector<byte> format_page(24);
	pages[3] = format_page;

	EnrichFormatPage(pages, false, SECTOR_SIZE);
	format_page = pages[3];
	EXPECT_EQ(byte{0}, format_page[12]);
	EXPECT_EQ(byte{0}, format_page[13]);

	EnrichFormatPage(pages, true, SECTOR_SIZE);
	format_page = pages[3];
	EXPECT_EQ(byte{SECTOR_SIZE >> 8}, format_page[12]);
	EXPECT_EQ(byte{0}, format_page[13]);
}

TEST(ScsiCommandUtilTest, AddAppleVendorModePage)
{
	map<int, vector<byte>> pages;
	vector<byte> vendor_page(30);
	pages[48] = vendor_page;

	AddAppleVendorModePage(pages, true);
	vendor_page = pages[48];
	EXPECT_EQ(byte{0}, vendor_page[2]);

	AddAppleVendorModePage(pages, false);
	vendor_page = pages[48];
	EXPECT_STREQ("APPLE COMPUTER, INC   ", (const char *)&vendor_page[2]);
}

TEST(ScsiCommandUtilTest, GetInt16)
{
	vector<int> v = { 0x12, 0x34 };
	EXPECT_EQ(0x1234, GetInt16(v, 0));
}

TEST(ScsiCommandUtilTest, GetInt24)
{
	vector<int> v = { 0x12, 0x34, 0x56 };
	EXPECT_EQ(0x123456, GetInt24(v, 0));
}

TEST(ScsiCommandUtilTest, GetInt32)
{
	vector<int> v = { 0x12, 0x34, 0x56, 0x78 };
	EXPECT_EQ(0x12345678, GetInt32(v, 0));
}

TEST(ScsiCommandUtilTest, GetInt64)
{
	vector<int> v = { 0x12, 0x34, 0x56, 0x78, 0x87, 0x65, 0x43, 0x21 };
	EXPECT_EQ(0x1234567887654321, GetInt64(v, 0));
}

TEST(ScsiCommandUtilTest, SetInt16)
{
	vector<byte> v(2);
	SetInt16(v, 0, 0x1234);
	EXPECT_EQ(byte{0x12}, v[0]);
	EXPECT_EQ(byte{0x34}, v[1]);
}

TEST(ScsiCommandUtilTest, SetInt32)
{
	vector<BYTE> buf(4);
	SetInt32(buf, 0, 0x12345678);
	EXPECT_EQ(0x12, buf[0]);
	EXPECT_EQ(0x34, buf[1]);
	EXPECT_EQ(0x56, buf[2]);
	EXPECT_EQ(0x78, buf[3]);

	vector<byte> v(4);
	SetInt32(v, 0, 0x12345678);
	EXPECT_EQ(byte{0x12}, v[0]);
	EXPECT_EQ(byte{0x34}, v[1]);
	EXPECT_EQ(byte{0x56}, v[2]);
	EXPECT_EQ(byte{0x78}, v[3]);
}

TEST(ScsiCommandUtilTest, SetInt64)
{
	vector<BYTE> buf(8);
	SetInt64(buf, 0, 0x1234567887654321);
	EXPECT_EQ(0x12, buf[0]);
	EXPECT_EQ(0x34, buf[1]);
	EXPECT_EQ(0x56, buf[2]);
	EXPECT_EQ(0x78, buf[3]);
	EXPECT_EQ(0x87, buf[4]);
	EXPECT_EQ(0x65, buf[5]);
	EXPECT_EQ(0x43, buf[6]);
	EXPECT_EQ(0x21, buf[7]);
}

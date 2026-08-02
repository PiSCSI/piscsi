//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/scsi.h"
#include "shared/piscsi_exceptions.h"
#include "devices/scsi_command_util.h"

using namespace scsi_command_util;

TEST(ScsiCommandUtilTest, ModeSelect6)
{
	const int LENGTH = 30;

	vector<int> cdb(6);
	vector<uint8_t> buf(LENGTH);

	// PF (vendor-specific parameter format) must not fail but be ignored
	cdb[1] = 0x00;
	ModeSelect(scsi_command::eCmdModeSelect6, cdb, buf, LENGTH, 0);

	cdb[0] = 0x15;
	// PF (standard parameter format)
	cdb[1] = 0x10;
	// Page 3 (Format Device Page), with its 22-byte payload
	buf[4] = 0x03;
	buf[5] = 0x16;
	// Request 512 bytes per sector
	buf[16] = 0x02;
	EXPECT_THAT([&] { ModeSelect(scsi_command::eCmdModeSelect6, cdb, buf, LENGTH, 256); },
			Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))))
		<< "Requested sector size does not match current sector size";

	// Page 0
	buf[4] = 0x00;
	EXPECT_THAT([&] { ModeSelect(scsi_command::eCmdModeSelect6, cdb, buf, LENGTH, 512); },
			Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))))
		<< "Unsupported page 0 was not rejected";

	// Page 3 (Format Device Page)
	buf[4] = 0x03;
	buf[16] = 0;
	EXPECT_THAT([&] { ModeSelect(scsi_command::eCmdModeSelect6, cdb, buf, LENGTH, 512); },
			Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))))
		<< "Requested sector size does not match current sector size";

	// Match the requested to the current sector size
	buf[16] = 0x02;
	EXPECT_THAT([&] { ModeSelect(scsi_command::eCmdModeSelect6, cdb, buf, LENGTH - 1, 512); },
			Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::parameter_list_length_error))))
		<< "Not enough command parameters";

	EXPECT_FALSE(ModeSelect(scsi_command::eCmdModeSelect6, cdb, buf, LENGTH, 512).empty());
}

TEST(ScsiCommandUtilTest, ModeSelect10)
{
	const int LENGTH = 34;

	vector<int> cdb(10);
	vector<uint8_t> buf(LENGTH);

	// PF (vendor-specific parameter format) must not fail but be ignored
	cdb[1] = 0x00;
	ModeSelect(scsi_command::eCmdModeSelect10, cdb, buf, LENGTH, 0);

	// PF (standard parameter format)
	cdb[1] = 0x10;
	// Page 3 (Format Device Page), with its 22-byte payload
	buf[8] = 0x03;
	buf[9] = 0x16;
	// Request 512 bytes per sector
	buf[20] = 0x02;
	EXPECT_THAT([&] { ModeSelect(scsi_command::eCmdModeSelect10, cdb, buf, LENGTH, 256); },
			Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))))
		<< "Requested sector size does not match current sector size";

	// Page 0
	buf[8] = 0x00;
	EXPECT_THAT([&] { ModeSelect(scsi_command::eCmdModeSelect10, cdb, buf, LENGTH, 512); },
			Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))))
		<< "Unsupported page 0 was not rejected";

	// Page 3 (Format Device Page)
	buf[8] = 0x03;
	buf[20] = 0;
	EXPECT_THAT([&] { ModeSelect(scsi_command::eCmdModeSelect10, cdb, buf, LENGTH, 512); },
			Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))))
		<< "Requested sector size does not match current sector size";

	// Match the requested to the current sector size
	buf[20] = 0x02;
	EXPECT_THAT([&] { ModeSelect(scsi_command::eCmdModeSelect10, cdb, buf, LENGTH - 1, 512); },
			Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::parameter_list_length_error))))
		<< "Not enough command parameters";

	EXPECT_FALSE(ModeSelect(scsi_command::eCmdModeSelect10, cdb, buf, LENGTH, 512).empty());
}

TEST(ScsiCommandUtilTest, ModeSelectRejectsTruncatedParameterList)
{
	vector<int> cdb(6);
	cdb[1] = 0x10;
	vector<uint8_t> buf(29);

	// A complete Format Device Page is followed by one byte of a truncated page header.
	buf[4] = 0x03;
	buf[5] = 0x16;
	buf[16] = 0x02;

	EXPECT_THAT([&] { ModeSelect(scsi_command::eCmdModeSelect6, cdb, buf, static_cast<int>(buf.size()), 512); },
			Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::parameter_list_length_error))))
		<< "Truncated mode page header was accepted";
}

TEST(ScsiCommandUtilTest, ModeSelectRejectsFormatDevicePageWithInvalidLength)
{
	vector<int> cdb(6);
	cdb[1] = 0x10;
	vector<uint8_t> buf(18);

	buf[4] = 0x03;
	buf[5] = 0x0c;
	buf[16] = 0x02;

	EXPECT_THAT([&] { ModeSelect(scsi_command::eCmdModeSelect6, cdb, buf, static_cast<int>(buf.size()), 512); },
			Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::invalid_field_in_parameter_list))))
		<< "Format Device Page with an invalid length was accepted";
}

TEST(ScsiCommandUtilTest, ModeSelectReportsParameterListLengthErrorForTruncation)
{
	const auto expect_truncation = [](const auto& operation) {
		EXPECT_THAT(operation, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::parameter_list_length_error))));
	};

	vector<int> cdb6(6);
	cdb6[1] = 0x10;
	vector<uint8_t> buf6(28);
	expect_truncation([&] { ModeSelect(scsi_command::eCmdModeSelect6, cdb6, buf6, static_cast<int>(buf6.size()) + 1, 512); });
	expect_truncation([&] { ModeSelect(scsi_command::eCmdModeSelect6, cdb6, buf6, 3, 512); });
	buf6[3] = 1;
	expect_truncation([&] { ModeSelect(scsi_command::eCmdModeSelect6, cdb6, buf6, 4, 512); });
	buf6[3] = 0;
	buf6[4] = 0x03;
	buf6[5] = 0x16;
	expect_truncation([&] { ModeSelect(scsi_command::eCmdModeSelect6, cdb6, buf6, 27, 512); });

	vector<int> cdb10(10);
	cdb10[1] = 0x10;
	vector<uint8_t> buf10(8);
	expect_truncation([&] { ModeSelect(scsi_command::eCmdModeSelect10, cdb10, buf10, 7, 512); });
	buf10[7] = 1;
	expect_truncation([&] { ModeSelect(scsi_command::eCmdModeSelect10, cdb10, buf10, 8, 512); });
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
	vector<uint8_t> b = { 0xfe, 0xdc };
	EXPECT_EQ(0xfedc, GetInt16(b, 0));

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
	vector<uint8_t> buf(4);
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
	vector<uint8_t> buf(8);
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

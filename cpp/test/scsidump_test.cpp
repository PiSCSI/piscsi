//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022 akuker
// Copyright (C) 2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>

#include "scsidump/scsidump_core.h"
#include "test/test_shared.h"

using namespace std;
using namespace filesystem;

TEST(ScsiDumpTest, GeneratePropertiesFile)
{
	// Basic test
	auto filename = CreateTempFile(0);
	ScsiDump::inquiry_info_t test_data = {
		.vendor = "PISCSI", .product = "TEST PRODUCT", .revision = "REV1", .sector_size = 1000, .capacity = 100
	};
	test_data.GeneratePropertiesFile(filename);

	string expected_str = "{\n"
	                      "   \"vendor\": \"PISCSI\",\n"
	                      "   \"product\": \"TEST PRODUCT\",\n"
	                      "   \"revision\": \"REV1\",\n"
	                      "   \"block_size\": \"1000\"\n}"
	                      "\n";
	EXPECT_EQ(expected_str, ReadTempFileToString(filename));

	// Long string test
	filename = CreateTempFile(0);
	test_data = {.vendor      = "01234567",
	             .product     = "0123456789ABCDEF",
	             .revision    = "0123",
	             .sector_size = UINT32_MAX,
	             .capacity    = UINT64_MAX
	            };
	test_data.GeneratePropertiesFile(filename);

	expected_str = "{\n"
	               "   \"vendor\": \"01234567\",\n"
	               "   \"product\": \"0123456789ABCDEF\",\n"
	               "   \"revision\": \"0123\",\n"
	               "   \"block_size\": \"4294967295\"\n"
	               "}\n";
	EXPECT_EQ(expected_str, ReadTempFileToString(filename));
	remove(filename);

	// Empty data test
	filename = CreateTempFile(0);
	test_data = {.vendor = "", .product = "", .revision = "", .sector_size = 0, .capacity = 0};
	test_data.GeneratePropertiesFile(filename);

	expected_str = "{\n"
	               "   \"vendor\": \"\",\n"
	               "   \"product\": \"\",\n"
	               "   \"revision\": \"\",\n"
	               "   \"block_size\": \"0\"\n"
	               "}\n";
	EXPECT_EQ(expected_str, ReadTempFileToString(filename));
	remove(filename);
}

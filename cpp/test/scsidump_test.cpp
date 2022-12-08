
//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022 akuker
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "scsidump/scsidump_core.h"
#include "test/test_shared.h"
#include <filesystem>
#include <fstream>

using namespace std;
using namespace filesystem;

class TestableScsidump : public RasDump
{
  public:
    static void PublicGeneratePropertiesFile(const string &filename, const inquiry_info_t &inq_info)
    {
        RasDump::GeneratePropertiesFile(filename, inq_info);
    }
};

TEST(ScsiDumpTest, GeneratePropertiesFile)
{
    // Basic test
    const string prop_file_name       = "test.properties";
    RasDump::inquiry_info_t test_data = {
        .vendor = "PISCSI", .product = "TEST PRODUCT", .revision = "REV1", .sector_size = 1000, .capacity = 100};
    TestableScsidump::PublicGeneratePropertiesFile("test", test_data);

    string expected_str = "{\n"
                          "   \"vendor\": \"PISCSI\",\n"
                          "   \"product\": \"TEST PRODUCT\",\n"
                          "   \"revision\": \"REV1\",\n"
                          "   \"block_size\": \"1000\",\n}"
                          "\n";
    EXPECT_EQ(ReadFileToString(prop_file_name), expected_str);

    // Long string test
    test_data = {.vendor      = "01234567",
                 .product     = "0123456789ABCDEF",
                 .revision    = "0123",
                 .sector_size = UINT32_MAX,
                 .capacity    = UINT64_MAX};
    TestableScsidump::PublicGeneratePropertiesFile("test", test_data);

    expected_str = "{\n"
                   "   \"vendor\": \"01234567\",\n"
                   "   \"product\": \"0123456789ABCDEF\",\n"
                   "   \"revision\": \"0123\",\n"
                   "   \"block_size\": \"4294967295\",\n"
                   "}\n";
    EXPECT_EQ(ReadFileToString(prop_file_name), expected_str);

    // Empty data test
    test_data = {.vendor = "", .product = "", .revision = "", .sector_size = 0, .capacity = 0};
    TestableScsidump::PublicGeneratePropertiesFile("test", test_data);

    expected_str = "{\n"
                   "   \"vendor\": \"\",\n"
                   "   \"product\": \"\",\n"
                   "   \"revision\": \"\",\n"
                   "   \"block_size\": \"0\",\n"
                   "}\n";
    EXPECT_EQ(ReadFileToString(prop_file_name), expected_str);
}

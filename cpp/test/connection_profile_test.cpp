//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
//---------------------------------------------------------------------------

#include "hal/connection_profile.h"

#include <gtest/gtest.h>

using namespace std;

TEST(ConnectionProfile, SelectsEachSupportedMapping)
{
    string error;

    ASSERT_TRUE(SetConnectionType("STANDARD", error));
    EXPECT_EQ(connection_type::standard, GetConnectionProfile().type);
    EXPECT_EQ(10, GetConnectionProfile().pins.data[0]);
    EXPECT_EQ(-1, GetConnectionProfile().pins.dtd);

    ASSERT_TRUE(SetConnectionType("gamernium", error));
    EXPECT_EQ(connection_type::gamernium, GetConnectionProfile().type);
    EXPECT_EQ(21, GetConnectionProfile().pins.data[0]);
    EXPECT_EQ(5, GetConnectionProfile().pins.dtd);

    ASSERT_TRUE(SetConnectionType("FULLSPEC", error));
    EXPECT_EQ(connection_type::fullspec, GetConnectionProfile().type);
    EXPECT_EQ(10, GetConnectionProfile().pins.data[0]);
    EXPECT_EQ(8, GetConnectionProfile().pins.dtd);
}

TEST(ConnectionProfile, RejectsUnsupportedMapping)
{
    string error;

    EXPECT_FALSE(SetConnectionType("AIBOM", error));
    EXPECT_EQ("Invalid connection type 'AIBOM'. Supported types are STANDARD, FULLSPEC, and GAMERNIUM", error);
}

TEST(ConnectionProfile, RemovesSelectionOptionFromArguments)
{
    char program[]     = "piscsi";
    char option[]      = "-C";
    char connection[]  = "GAMERNIUM";
    char argument[]    = "-r";
    char value[]       = "7";
    vector<char*> args = {program, option, connection, argument, value};
    string error;

    ASSERT_TRUE(ConfigureConnectionType(args, error));
    ASSERT_EQ(3U, args.size());
    EXPECT_STREQ("piscsi", args[0]);
    EXPECT_STREQ("-r", args[1]);
    EXPECT_STREQ("7", args[2]);
    EXPECT_EQ(connection_type::gamernium, GetConnectionProfile().type);

    ASSERT_TRUE(SetConnectionType("FULLSPEC", error));
}

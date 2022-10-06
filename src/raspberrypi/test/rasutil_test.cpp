//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "rasutil.h"

using namespace ras_util;

TEST(RasUtilTest, GetAsInt)
{
	int result;

	EXPECT_FALSE(GetAsInt("", result));
	EXPECT_FALSE(GetAsInt("xyz", result));
	EXPECT_FALSE(GetAsInt("-1", result));
	EXPECT_FALSE(GetAsInt("1234567898765432112345678987654321", result)) << "Value is out of range";
	EXPECT_TRUE(GetAsInt("0", result));
	EXPECT_EQ(0, result);
	EXPECT_TRUE(GetAsInt("1234", result));
	EXPECT_EQ(1234, result);
}

TEST(RasUtilTest, Banner)
{
	EXPECT_FALSE(Banner("Test").empty());
}

TEST(RasUtilTest, ListDevices)
{
	list<PbDevice> devices;

	EXPECT_FALSE(ListDevices(devices).empty());

	PbDevice device;
	devices.push_back(device);
	EXPECT_FALSE(ListDevices(devices).empty());
}

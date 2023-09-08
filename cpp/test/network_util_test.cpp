//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>

#include "shared/network_util.h"

using namespace network_util;

TEST(NetworkUtilTest, IsInterfaceUp)
{
	EXPECT_FALSE(IsInterfaceUp("foo_bar"));
}

TEST(NetworkUtilTest, GetNetworkInterfaces)
{
	EXPECT_FALSE(GetNetworkInterfaces().empty());
}
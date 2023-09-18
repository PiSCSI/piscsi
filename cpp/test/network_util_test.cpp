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
#include <netdb.h>

using namespace network_util;

TEST(NetworkUtilTest, IsInterfaceUp)
{
	EXPECT_FALSE(IsInterfaceUp("foo_bar"));
}

TEST(NetworkUtilTest, GetNetworkInterfaces)
{
	EXPECT_FALSE(GetNetworkInterfaces().empty());
}

TEST(NetworkUtilTest, ResolveHostName)
{
	sockaddr_in server_addr = {};
	EXPECT_FALSE(ResolveHostName("foo.foobar", &server_addr));
	EXPECT_TRUE(ResolveHostName("127.0.0.1", &server_addr));
}

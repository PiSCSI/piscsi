//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>

#include <netdb.h>
#include <netinet/in.h>
#include <fstream>
#include "shared/network_util.h"
#include "test_shared.h"

using namespace network_util;

TEST(NetworkUtilTest, IsInterfaceUp)
{
	EXPECT_FALSE(IsInterfaceUp("foo_bar"));
}

TEST(NetworkUtilTest, IsValidInterfaceName)
{
	EXPECT_TRUE(IsValidInterfaceName("enp0s31f6"));
	EXPECT_TRUE(IsValidInterfaceName("wlx001122334455"));
	EXPECT_FALSE(IsValidInterfaceName(""));
	EXPECT_FALSE(IsValidInterfaceName("wlan0,wlan1"));
	EXPECT_FALSE(IsValidInterfaceName("../wlan0"));
}

TEST(NetworkUtilTest, ParseProxyArpUplink)
{
	EXPECT_EQ("wlan0", ParseProxyArpUplink("PISCSI_PROXYARP_UPLINK=wlan0\n").value());
	EXPECT_EQ("wlx001122334455", ParseProxyArpUplink("# comment\nPISCSI_PROXYARP_UPLINK=wlx001122334455\n").value());
	EXPECT_FALSE(ParseProxyArpUplink("PISCSI_PROXYARP_UPLINK=wlan0\nPISCSI_PROXYARP_UPLINK=wlan1\n").has_value());
	EXPECT_FALSE(ParseProxyArpUplink("PISCSI_PROXYARP_UPLINK=wlan0,wlan1\n").has_value());
	EXPECT_FALSE(ParseProxyArpUplink("PISCSI_PROXYARP_UPLINK=\n").has_value());
	EXPECT_FALSE(ParseProxyArpUplink("PISCSI_PROXYARP_PROMISC=true\n").has_value());
}

TEST(NetworkUtilTest, ReadProxyArpUplinkFile)
{
	const auto configuration = test_data_temp_path / "proxyarp-network.conf";
	const auto directory = test_data_temp_path / "proxyarp-network.conf-directory";
	filesystem::create_directories(test_data_temp_path);
	filesystem::remove(configuration);
	filesystem::remove_all(directory);
	ASSERT_TRUE(filesystem::create_directory(directory));
	EXPECT_FALSE(ReadProxyArpUplinkFile(directory).has_value());

	{
		ofstream output(configuration);
		output << "PISCSI_PROXYARP_UPLINK=wlan0\n";
	}
	EXPECT_EQ("wlan0", ReadProxyArpUplinkFile(configuration).value());

	filesystem::remove(configuration);
	filesystem::remove_all(directory);
}

TEST(NetworkUtilTest, ClassifyInterface)
{
	EXPECT_EQ(NetworkInterfaceType::LOOPBACK, ClassifyInterface(false, false, false, false, "lo"));
	EXPECT_EQ(NetworkInterfaceType::BRIDGE, ClassifyInterface(false, true, false, true, "piscsi_bridge"));
	EXPECT_EQ(NetworkInterfaceType::WIFI, ClassifyInterface(true, false, false, false, "wlan0"));
	EXPECT_EQ(NetworkInterfaceType::TAP, ClassifyInterface(false, false, true, true, "piscsi0"));
	EXPECT_EQ(NetworkInterfaceType::VIRTUAL, ClassifyInterface(false, false, false, true, "veth0"));
	EXPECT_EQ(NetworkInterfaceType::ETHERNET, ClassifyInterface(false, false, false, false, "enp1s0"));
}

TEST(NetworkUtilTest, GetNetworkInterfaces)
{
#ifdef __linux__
	for (const auto& [name, interface] : GetNetworkInterfaceInfo()) {
		EXPECT_EQ(name, interface.name);
	}
#else
	GetNetworkInterfaces();
#endif
}

TEST(NetworkUtilTest, ResolveHostName)
{
	sockaddr_in server_addr = {};
	EXPECT_FALSE(ResolveHostName("foo.foobar", &server_addr));
	EXPECT_TRUE(ResolveHostName("127.0.0.1", &server_addr));
}

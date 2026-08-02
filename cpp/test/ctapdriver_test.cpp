//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include <net/ethernet.h>
#include "devices/ctapdriver.h"

using namespace network_util;

TEST(CTapDriverTest, GetDefaultParams)
{
	CTapDriver driver;
	const auto params = driver.GetDefaultParams();
	EXPECT_EQ(2, params.size());
	EXPECT_EQ("piscsi_bridge", params.at("interface"));
	EXPECT_EQ("bridge", params.at("mode"));
	EXPECT_FALSE(params.contains("inet"));
}

TEST(CTapDriverTest, ValidateProfiles)
{
	const network_interface_map interfaces = {
		{ "piscsi_bridge", { "piscsi_bridge", NetworkInterfaceType::BRIDGE, true } },
		{ "wlan0", { "wlan0", NetworkInterfaceType::WIFI, true } },
		{ "eth0", { "eth0", NetworkInterfaceType::ETHERNET, true } },
		{ "wlan1", { "wlan1", NetworkInterfaceType::WIFI, false } },
	};

	EXPECT_TRUE(CTapDriver::GetProfileValidationError("bridge", "piscsi_bridge", interfaces).empty());
	EXPECT_TRUE(CTapDriver::GetProfileValidationError("proxyarp", "wlan0", interfaces).empty());
	EXPECT_THAT(CTapDriver::GetProfileValidationError("", "piscsi_bridge", interfaces), HasSubstr("Unsupported"));
	EXPECT_THAT(CTapDriver::GetProfileValidationError("bridge", "eth0", interfaces), HasSubstr("requires"));
	EXPECT_THAT(CTapDriver::GetProfileValidationError("proxyarp", "eth0", interfaces), HasSubstr("Wi-Fi"));
	EXPECT_THAT(CTapDriver::GetProfileValidationError("proxyarp", "wlan1", interfaces), HasSubstr("down"));
	EXPECT_THAT(CTapDriver::GetProfileValidationError("proxyarp", "wlan0,eth0", interfaces), HasSubstr("Invalid"));

	auto conflicting_interfaces = interfaces;
	conflicting_interfaces.emplace("piscsi0", NetworkInterfaceInfo { "piscsi0", NetworkInterfaceType::TAP, true });
	EXPECT_THAT(CTapDriver::GetProfileValidationError("proxyarp", "wlan0", conflicting_interfaces), HasSubstr("in use"));
}

TEST(CTapDriverTest, CleanupAndMacAreSafeWithoutATap)
{
	CTapDriver driver;
	array<uint8_t, 6> mac;
	mac.fill(0xff);
	driver.GetMacAddr(mac.data());
	EXPECT_THAT(mac, Each(0));

	driver.CleanUp();
	driver.CleanUp();
}

TEST(CTapDriverTest, DeriveDaynaPortMac)
{
	const array<uint8_t, 6> interface_mac = { 0xb8, 0x27, 0xeb, 0xb5, 0xa3, 0x15 };
	EXPECT_EQ((array<uint8_t, 6> { 0x00, 0x80, 0x19, 0xb5, 0xa3, 0x15 }),
			CTapDriver::DeriveDaynaPortMac(interface_mac));

	const array<uint8_t, 5> invalid_mac = { 0, 1, 2, 3, 4 };
	EXPECT_EQ((array<uint8_t, 6> {}), CTapDriver::DeriveDaynaPortMac(invalid_mac));
}

TEST(CTapDriverTest, Crc32)
{
	array<uint8_t, ETH_FRAME_LEN> buf;

	buf.fill(0x00);
	EXPECT_EQ(0xe3d887bb, CTapDriver::Crc32(span(buf.data(), ETH_FRAME_LEN)));

	buf.fill(0xff);
	EXPECT_EQ(0x814765f4, CTapDriver::Crc32(span(buf.data(), ETH_FRAME_LEN)));

	buf.fill(0x10);
	EXPECT_EQ(0xb7288Cd3, CTapDriver::Crc32(span(buf.data(), ETH_FRAME_LEN)));

	buf.fill(0x7f);
	EXPECT_EQ(0x4b543477, CTapDriver::Crc32(span(buf.data(), ETH_FRAME_LEN)));

	buf.fill(0x80);
	EXPECT_EQ(0x29cbd638, CTapDriver::Crc32(span(buf.data(), ETH_FRAME_LEN)));

	for (size_t i = 0; i < buf.size(); i++) {
		buf[i] = (uint8_t)i;
	}
	EXPECT_EQ(0xe7870705, CTapDriver::Crc32(span(buf.data(), ETH_FRAME_LEN)));

	for (size_t i = buf.size() - 1; i > 0; i--) {
		buf[i] = (uint8_t)i;
	}
	EXPECT_EQ(0xe7870705, CTapDriver::Crc32(span(buf.data(), ETH_FRAME_LEN)));
}

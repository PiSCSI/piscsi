//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
//---------------------------------------------------------------------------

#include "devices/scsi_host_bridge.h"
#include "mocks.h"

TEST(ScsiHostBridgeTest, GetDefaultParams)
{
    const auto [controller, bridge] = CreateDevice(SCBR);
    const auto params               = bridge->GetDefaultParams();
    EXPECT_EQ(2, params.size());
}

TEST(ScsiHostBridgeTest, DeriveMac)
{
	const array<uint8_t, 6> uplink_mac = { 0xb8, 0x27, 0xeb, 0xb5, 0xa3, 0x15 };
	const auto mac = SCSIBR::DeriveMac(uplink_mac);
	EXPECT_EQ(0x02, mac[0]);
	EXPECT_NE(uplink_mac, mac);
	EXPECT_EQ(mac, SCSIBR::DeriveMac(uplink_mac));

	const array<uint8_t, 5> invalid_mac = { 0, 1, 2, 3, 4 };
	EXPECT_EQ((array<uint8_t, 6> {}), SCSIBR::DeriveMac(invalid_mac));
}

TEST(ScsiHostBridgeTest, InquiryDisablesRasctlControlByDefault)
{
	const auto [controller, bridge] = CreateDevice(SCBR);
	const auto inquiry              = std::dynamic_pointer_cast<SCSIBR>(bridge)->InquiryInternal();
	EXPECT_EQ(0, inquiry[36]);
	EXPECT_EQ('1', inquiry[38]);
}

TEST(ScsiHostBridgeTest, RasctlControlUsesRasctlMessageFormat)
{
	const auto [controller, device] = CreateDevice(SCBR);
	const auto bridge               = std::dynamic_pointer_cast<SCSIBR>(device);
	const array<int, 10> write_cdb  = { 0x2a, 0, 0, 0, 0, 0, 0, 4, 0, 0 };
	const array<int, 10> read_cdb   = { 0x28, 0, 0, 0, 0, 0, 0, 4, 0, 0 };
	vector<uint8_t> buffer(1024);
	EXPECT_FALSE(bridge->ReadWrite(write_cdb, buffer));
	EXPECT_EQ(0, bridge->GetMessage10(read_cdb, buffer));
	bridge->SetRasctlControlMode(SCSIBR::rasctl_control_mode::media);
	EXPECT_EQ('1', bridge->InquiryInternal()[36]);

	memcpy(buffer.data(), "list\n", 5);
	EXPECT_TRUE(bridge->ReadWrite(write_cdb, buffer));
	const auto command = bridge->TakeRasctlControlRequest();
	ASSERT_TRUE(command.has_value());
	EXPECT_EQ("list\n", *command);

	bridge->SetRasctlControlResponse("No devices currently attached.\n");
	buffer.assign(buffer.size(), 0xff);
	EXPECT_EQ(1024, bridge->GetMessage10(read_cdb, buffer));
	EXPECT_STREQ("No devices currently attached.\n", reinterpret_cast<const char *>(buffer.data()));

	bridge->SetRasctlControlResponse("PiSCSI shutdown requested\n", SCSIBR::rasctl_shutdown_mode::stop_piscsi);
	EXPECT_EQ(1024, bridge->GetMessage10(read_cdb, buffer));
	EXPECT_EQ(SCSIBR::rasctl_shutdown_mode::stop_piscsi, bridge->TakeRasctlShutdownMode());
}

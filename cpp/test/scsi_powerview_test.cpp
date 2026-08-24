//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "devices/scsi_powerview.h"
#include "shared/piscsi_exceptions.h"

TEST(ScsiPowerViewTest, Inquiry)
{
	auto [controller, device] = CreateDevice(SCPV);

	controller->SetCmdByte(3, 0);
	controller->SetCmdByte(4, 0x4b);
	EXPECT_CALL(*controller, DataIn());
	device->Dispatch(scsi_command::eCmdInquiry);

	const auto& buffer = controller->GetBuffer();
	EXPECT_EQ(0x4b, controller->GetLength());
	EXPECT_EQ(0x03, buffer[0]);
	EXPECT_EQ(0x46, buffer[4]);
	EXPECT_EQ("RADIUS  ", string(buffer.begin() + 8, buffer.begin() + 16));
	EXPECT_EQ("PowerView       ", string(buffer.begin() + 16, buffer.begin() + 32));
	EXPECT_EQ("V1.0", string(buffer.begin() + 32, buffer.begin() + 36));
}

TEST(ScsiPowerViewTest, ReadConfiguration)
{
	auto [controller, device] = CreateDevice(SCPV);

	controller->SetCmdByte(3, 0x31);
	controller->SetCmdByte(4, 0x00);
	controller->SetCmdByte(6, 3);
	EXPECT_CALL(*controller, DataIn());
	device->Dispatch(scsi_command::eCmdPowerViewReadConfig);
	EXPECT_EQ(3, controller->GetLength());
	EXPECT_EQ(0x01, controller->GetBuffer()[0]);
	EXPECT_EQ(0x09, controller->GetBuffer()[1]);
	EXPECT_EQ(0x08, controller->GetBuffer()[2]);

	controller->SetCmdByte(4, 0x82);
	controller->SetCmdByte(6, 1);
	EXPECT_CALL(*controller, DataIn());
	device->Dispatch(scsi_command::eCmdPowerViewReadConfig);
	EXPECT_EQ(1, controller->GetLength());
	EXPECT_EQ(0x01, controller->GetBuffer()[0]);

	controller->SetCmdByte(4, 0x83);
	EXPECT_CALL(*controller, DataIn());
	device->Dispatch(scsi_command::eCmdPowerViewReadConfig);
	EXPECT_EQ(0x00, controller->GetBuffer()[0]);
}

TEST(ScsiPowerViewTest, DataOutCommands)
{
	auto [controller, device] = CreateDevice(SCPV);
	auto power_view = dynamic_pointer_cast<SCSIPowerView>(device);

	controller->SetCmdByte(6, 4);
	EXPECT_CALL(*controller, DataOut());
	device->Dispatch(scsi_command::eCmdPowerViewWriteConfig);
	EXPECT_TRUE(controller->IsByteTransfer());
	EXPECT_EQ(4, controller->GetLength());
	EXPECT_TRUE(power_view->WriteByteSequence(vector<uint8_t>{ 0x00, 0x01, 0x01, 0x01 }));
	EXPECT_FALSE(power_view->WriteByteSequence({}));

	controller->SetCmdByte(3, 0);
	controller->SetCmdByte(4, 1);
	EXPECT_CALL(*controller, DataOut());
	device->Dispatch(scsi_command::eCmdPowerViewWriteColorPalette);
	EXPECT_EQ(8, controller->GetLength());
	EXPECT_TRUE(power_view->WriteByteSequence(vector<uint8_t>(8)));

	controller->SetCmdByte(4, 0);
	controller->SetCmdByte(5, 80);
	controller->SetCmdByte(6, 1);
	controller->SetCmdByte(7, 0x90);
	EXPECT_CALL(*controller, DataOut());
	device->Dispatch(scsi_command::eCmdPowerViewWriteFrameBuffer);
	EXPECT_EQ(80 * 400, controller->GetLength());
	EXPECT_TRUE(power_view->WriteByteSequence(vector<uint8_t>(80 * 400)));

	EXPECT_CALL(*controller, DataOut());
	device->Dispatch(scsi_command::eCmdPowerViewUnknownCC);
	EXPECT_EQ(SCSIPowerView::UNKNOWN_CC_LENGTH, controller->GetLength());
	EXPECT_TRUE(power_view->WriteByteSequence(vector<uint8_t>(SCSIPowerView::UNKNOWN_CC_LENGTH)));
}

TEST(ScsiPowerViewTest, RejectsInvalidDataOutLengths)
{
	auto [controller, device] = CreateDevice(SCPV);

	controller->SetCmdByte(3, 1);
	controller->SetCmdByte(4, 1);
	EXPECT_THAT([&] { device->Dispatch(scsi_command::eCmdPowerViewWriteColorPalette); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))));

	controller->SetCmdByte(4, 0x03);
	controller->SetCmdByte(5, 0x21);
	controller->SetCmdByte(6, 0x02);
	controller->SetCmdByte(7, 0x58);
	EXPECT_THAT([&] { device->Dispatch(scsi_command::eCmdPowerViewWriteFrameBuffer); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))));
}

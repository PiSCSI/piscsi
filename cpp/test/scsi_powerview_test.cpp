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

TEST(ScsiPowerViewTest, DecodesPaletteAndFrameBufferUpdates)
{
	auto [controller, device] = CreateDevice(SCPV);
	auto power_view = dynamic_pointer_cast<SCSIPowerView>(device);

	controller->SetCmdByte(3, 0);
	controller->SetCmdByte(4, 1);
	EXPECT_CALL(*controller, DataOut());
	device->Dispatch(scsi_command::eCmdPowerViewWriteColorPalette);
	EXPECT_TRUE(power_view->WriteByteSequence(vector<uint8_t>{
			0x00, 0x01, 0x02, 0x03, 0x80, 0x04, 0x05, 0x06
	}));
	EXPECT_EQ(0x01, power_view->GetPalette()[0x00][0]);
	EXPECT_EQ(0x05, power_view->GetPalette()[0x80][1]);

	controller->SetCmdByte(1, 0);
	controller->SetCmdByte(2, 0);
	controller->SetCmdByte(3, 0);
	controller->SetCmdByte(4, 0);
	controller->SetCmdByte(5, 1);
	controller->SetCmdByte(6, 0);
	controller->SetCmdByte(7, 1);
	EXPECT_CALL(*controller, DataOut());
	device->Dispatch(scsi_command::eCmdPowerViewWriteFrameBuffer);
	EXPECT_TRUE(power_view->WriteByteSequence(vector<uint8_t>{ 0x81 }));
	EXPECT_EQ(0x00, power_view->GetPixel(0, 0));
	EXPECT_EQ(0x80, power_view->GetPixel(1, 0));
	EXPECT_EQ(0x00, power_view->GetPixel(7, 0));

	controller->SetCmdByte(3, 0);
	controller->SetCmdByte(4, 16);
	EXPECT_CALL(*controller, DataOut());
	device->Dispatch(scsi_command::eCmdPowerViewWriteColorPalette);
	vector<uint8_t> sixteen_color_palette(16 * 4);
	for (uint8_t index = 0; index < 16; ++index) {
		sixteen_color_palette[index * 4] = index;
		sixteen_color_palette[index * 4 + 1] = index;
		sixteen_color_palette[index * 4 + 2] = index + 1;
		sixteen_color_palette[index * 4 + 3] = index + 2;
	}
	EXPECT_TRUE(power_view->WriteByteSequence(sixteen_color_palette));
	EXPECT_EQ(0x0c, power_view->GetPalette()[0x0a][2]);

	controller->SetCmdByte(3, 0);
	controller->SetCmdByte(4, 0);
	controller->SetCmdByte(5, 1);
	controller->SetCmdByte(6, 0);
	controller->SetCmdByte(7, 1);
	EXPECT_CALL(*controller, DataOut());
	device->Dispatch(scsi_command::eCmdPowerViewWriteFrameBuffer);
	EXPECT_TRUE(power_view->WriteByteSequence(vector<uint8_t>{ 0xa5 }));
	EXPECT_EQ(0x0a, power_view->GetPixel(0, 0));
	EXPECT_EQ(0x05, power_view->GetPixel(1, 0));

	controller->SetCmdByte(3, 1);
	controller->SetCmdByte(4, 0);
	EXPECT_CALL(*controller, DataOut());
	device->Dispatch(scsi_command::eCmdPowerViewWriteColorPalette);
	vector<uint8_t> two_hundred_fifty_six_color_palette(256 * 4);
	for (size_t index = 0; index < 256; ++index) {
		two_hundred_fifty_six_color_palette[index * 4] = static_cast<uint8_t>(index);
	}
	EXPECT_TRUE(power_view->WriteByteSequence(two_hundred_fifty_six_color_palette));

	controller->SetCmdByte(3, 0);
	controller->SetCmdByte(4, 0);
	controller->SetCmdByte(5, 2);
	EXPECT_CALL(*controller, DataOut());
	device->Dispatch(scsi_command::eCmdPowerViewWriteFrameBuffer);
	EXPECT_TRUE(power_view->WriteByteSequence(vector<uint8_t>{ 0x12, 0x34 }));
	EXPECT_EQ(0x12, power_view->GetPixel(0, 0));
	EXPECT_EQ(0x34, power_view->GetPixel(1, 0));
}

TEST(ScsiPowerViewTest, AppliesOffsetUpdatesToCapturedModes)
{
	auto [controller, device] = CreateDevice(SCPV);
	auto power_view = dynamic_pointer_cast<SCSIPowerView>(device);

	controller->SetCmdByte(3, 0);
	controller->SetCmdByte(4, 1);
	EXPECT_CALL(*controller, DataOut());
	device->Dispatch(scsi_command::eCmdPowerViewWriteColorPalette);
	EXPECT_TRUE(power_view->WriteByteSequence(vector<uint8_t>{
			0x00, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00
	}));

	controller->SetCmdByte(1, 0);
	controller->SetCmdByte(2, 0);
	controller->SetCmdByte(3, 0);
	controller->SetCmdByte(4, 0);
	controller->SetCmdByte(5, 80);
	controller->SetCmdByte(6, 0x01);
	controller->SetCmdByte(7, 0xe0);
	EXPECT_CALL(*controller, DataOut());
	device->Dispatch(scsi_command::eCmdPowerViewWriteFrameBuffer);
	EXPECT_TRUE(power_view->WriteByteSequence(vector<uint8_t>(80 * 480)));
	EXPECT_EQ(640, power_view->GetScreenWidth());
	EXPECT_EQ(480, power_view->GetScreenHeight());
	EXPECT_EQ(0x80, power_view->GetPixel(15, 10));

	// The 1-bit CDB address is word-addressed: 0x0644 maps to byte 802, row 10, column 16.
	controller->SetCmdByte(1, 0x00);
	controller->SetCmdByte(2, 0x06);
	controller->SetCmdByte(3, 0x44);
	controller->SetCmdByte(4, 0);
	controller->SetCmdByte(5, 1);
	controller->SetCmdByte(6, 0);
	controller->SetCmdByte(7, 1);
	EXPECT_CALL(*controller, DataOut());
	device->Dispatch(scsi_command::eCmdPowerViewWriteFrameBuffer);
	EXPECT_TRUE(power_view->WriteByteSequence(vector<uint8_t>{ 0x80 }));
	EXPECT_EQ(0x00, power_view->GetPixel(16, 10));
	EXPECT_EQ(0x80, power_view->GetPixel(23, 10));
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

	controller->SetCmdByte(1, 0);
	controller->SetCmdByte(2, 0);
	controller->SetCmdByte(3, 1);
	controller->SetCmdByte(4, 0);
	controller->SetCmdByte(5, 1);
	controller->SetCmdByte(6, 0);
	controller->SetCmdByte(7, 1);
	EXPECT_THAT([&] { device->Dispatch(scsi_command::eCmdPowerViewWriteFrameBuffer); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))));
}

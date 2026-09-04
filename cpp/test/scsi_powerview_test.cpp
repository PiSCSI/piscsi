//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "devices/scsi_powerview.h"
#include "shared/piscsi_exceptions.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <thread>

using namespace filesystem;

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

TEST(ScsiPowerViewTest, Read6ReportsNoMediumForRadiusWareProbe)
{
	auto [controller, device] = CreateDevice(SCPV);

	// The SE/30's RadiusWare startup probe is READ(6), LBA 0, one block:
	// 08 00 00 00 01 00. The legacy PowerView reports an empty medium rather
	// than an unsupported opcode, letting the driver continue to its vendor CDBs.
	EXPECT_THAT([&] { device->Dispatch(scsi_command::eCmdRead6); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::not_ready),
			Property(&scsi_exception::get_asc, asc::medium_not_present))));
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
	EXPECT_EQ(4, controller->GetLength());
	EXPECT_TRUE(power_view->WriteByteSequence(vector<uint8_t>(4)));

	controller->SetCmdByte(4, 0);
	controller->SetCmdByte(5, 80);
	controller->SetCmdByte(6, 1);
	controller->SetCmdByte(7, 0x90);
	EXPECT_CALL(*controller, DataOut());
	device->Dispatch(scsi_command::eCmdPowerViewWriteFrameBuffer);
	EXPECT_EQ(80 * 400, controller->GetLength());
	EXPECT_TRUE(power_view->WriteByteSequence(vector<uint8_t>(80 * 400)));

	controller->SetCmdByte(1, 0x45);
	controller->SetCmdByte(2, 0xe0);
	EXPECT_CALL(*controller, DataOut());
	device->Dispatch(scsi_command::eCmdPowerViewUnknownCC);
	EXPECT_EQ(0x8bc, controller->GetLength());
	EXPECT_TRUE(power_view->WriteByteSequence(vector<uint8_t>(0x8bc)));
}

TEST(ScsiPowerViewTest, DecodesPaletteAndFrameBufferUpdates)
{
	auto [controller, device] = CreateDevice(SCPV);
	auto power_view = dynamic_pointer_cast<SCSIPowerView>(device);

	controller->SetCmdByte(3, 0);
	controller->SetCmdByte(4, 2);
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
	EXPECT_EQ(0x80, power_view->GetPixel(0, 0));
	EXPECT_EQ(0x00, power_view->GetPixel(1, 0));
	EXPECT_EQ(0x80, power_view->GetPixel(7, 0));

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

TEST(ScsiPowerViewTest, UsesVisibleMonochromeDefaultsBeforePaletteUpdate)
{
	auto [controller, device] = CreateDevice(SCPV);
	auto power_view = dynamic_pointer_cast<SCSIPowerView>(device);

	EXPECT_EQ((SCSIPowerView::color_t { 0xff, 0xff, 0xff }), power_view->GetPalette()[0x00]);
	EXPECT_EQ((SCSIPowerView::color_t { 0x00, 0x00, 0x00 }), power_view->GetPalette()[0x80]);

	controller->SetCmdByte(1, 0);
	controller->SetCmdByte(2, 0);
	controller->SetCmdByte(3, 0);
	controller->SetCmdByte(4, 0);
	controller->SetCmdByte(5, 1);
	controller->SetCmdByte(6, 0);
	controller->SetCmdByte(7, 1);
	EXPECT_CALL(*controller, DataOut());
	device->Dispatch(scsi_command::eCmdPowerViewWriteFrameBuffer);
	EXPECT_TRUE(power_view->WriteByteSequence(vector<uint8_t>{ 0x80 }));
	EXPECT_EQ(0x80, power_view->GetPixel(0, 0));
	EXPECT_EQ(0x00, power_view->GetPixel(1, 0));
}

TEST(ScsiPowerViewTest, AppliesOffsetUpdatesToCapturedModes)
{
	auto [controller, device] = CreateDevice(SCPV);
	auto power_view = dynamic_pointer_cast<SCSIPowerView>(device);

	controller->SetCmdByte(3, 0);
	controller->SetCmdByte(4, 2);
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
	EXPECT_EQ(0x00, power_view->GetPixel(15, 10));

	// Captured command CA 01 1E A8 00 01 00 03, with data FFFFFF.
	// Its 1-bit word address maps to byte 36692, row 458, column 416.
	controller->SetCmdByte(1, 0x01);
	controller->SetCmdByte(2, 0x1e);
	controller->SetCmdByte(3, 0xa8);
	controller->SetCmdByte(4, 0);
	controller->SetCmdByte(5, 1);
	controller->SetCmdByte(6, 0);
	controller->SetCmdByte(7, 3);
	EXPECT_CALL(*controller, DataOut());
	device->Dispatch(scsi_command::eCmdPowerViewWriteFrameBuffer);
	EXPECT_TRUE(power_view->WriteByteSequence(vector<uint8_t>{ 0xff, 0xff, 0xff }));
	EXPECT_EQ(0x00, power_view->GetPixel(415, 458));
	EXPECT_EQ(0x80, power_view->GetPixel(416, 458));
	EXPECT_EQ(0x80, power_view->GetPixel(423, 460));
	EXPECT_EQ(0x00, power_view->GetPixel(416, 461));
}

TEST(ScsiPowerViewTest, SupportsCapturedModesAndSurfaceEdges)
{
	auto [controller, device] = CreateDevice(SCPV);
	auto power_view = dynamic_pointer_cast<SCSIPowerView>(device);

	controller->SetCmdByte(3, 0);
	controller->SetCmdByte(4, 2);
	EXPECT_CALL(*controller, DataOut());
	device->Dispatch(scsi_command::eCmdPowerViewWriteColorPalette);
	EXPECT_TRUE(power_view->WriteByteSequence(vector<uint8_t>{
			0x00, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00
	}));

	// Captured full-refresh CDB: CA 00 00 00 00 50 01 90 (640 by 400, 1 bit).
	controller->SetCmdByte(1, 0);
	controller->SetCmdByte(2, 0);
	controller->SetCmdByte(3, 0);
	controller->SetCmdByte(4, 0);
	controller->SetCmdByte(5, 0x50);
	controller->SetCmdByte(6, 0x01);
	controller->SetCmdByte(7, 0x90);
	EXPECT_CALL(*controller, DataOut());
	device->Dispatch(scsi_command::eCmdPowerViewWriteFrameBuffer);
	EXPECT_TRUE(power_view->WriteByteSequence(vector<uint8_t>(80 * 400, 0xff)));
	EXPECT_EQ(640, power_view->GetScreenWidth());
	EXPECT_EQ(400, power_view->GetScreenHeight());
	EXPECT_EQ(0x80, power_view->GetPixel(639, 399));

	controller->SetCmdByte(3, 1);
	controller->SetCmdByte(4, 0);
	EXPECT_CALL(*controller, DataOut());
	device->Dispatch(scsi_command::eCmdPowerViewWriteColorPalette);
	vector<uint8_t> palette(256 * 4);
	for (size_t index = 0; index < 256; ++index) {
		palette[index * 4] = static_cast<uint8_t>(index);
	}
	EXPECT_TRUE(power_view->WriteByteSequence(palette));

	// Captured full-refresh CDB: CA 00 00 00 03 20 02 58 (800 by 600, 8 bit).
	controller->SetCmdByte(1, 0);
	controller->SetCmdByte(2, 0);
	controller->SetCmdByte(3, 0);
	controller->SetCmdByte(4, 0x03);
	controller->SetCmdByte(5, 0x20);
	controller->SetCmdByte(6, 0x02);
	controller->SetCmdByte(7, 0x58);
	vector<uint8_t> full_refresh(800 * 600);
	full_refresh[0] = 0x12;
	full_refresh.back() = 0x34;
	EXPECT_CALL(*controller, DataOut());
	device->Dispatch(scsi_command::eCmdPowerViewWriteFrameBuffer);
	EXPECT_TRUE(power_view->WriteByteSequence(full_refresh));
	EXPECT_EQ(800, power_view->GetScreenWidth());
	EXPECT_EQ(600, power_view->GetScreenHeight());
	EXPECT_EQ(0x12, power_view->GetPixel(0, 0));
	EXPECT_EQ(0x34, power_view->GetPixel(799, 599));

	// A two-pixel update is valid at the final two pixels of the surface.
	controller->SetCmdByte(1, 0x07);
	controller->SetCmdByte(2, 0x52);
	controller->SetCmdByte(3, 0xfe);
	controller->SetCmdByte(4, 0);
	controller->SetCmdByte(5, 2);
	controller->SetCmdByte(6, 0);
	controller->SetCmdByte(7, 1);
	EXPECT_CALL(*controller, DataOut());
	device->Dispatch(scsi_command::eCmdPowerViewWriteFrameBuffer);
	EXPECT_TRUE(power_view->WriteByteSequence(vector<uint8_t>{ 0x56, 0x78 }));
	EXPECT_EQ(0x56, power_view->GetPixel(798, 599));
	EXPECT_EQ(0x78, power_view->GetPixel(799, 599));

	controller->SetCmdByte(3, 0xff);
	EXPECT_THAT([&] { device->Dispatch(scsi_command::eCmdPowerViewWriteFrameBuffer); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))));
}

TEST(ScsiPowerViewTest, UsesCaControlByteToIdentifyFullRefreshes)
{
	auto [controller, device] = CreateDevice(SCPV);
	auto power_view = dynamic_pointer_cast<SCSIPowerView>(device);

	controller->SetCmdByte(3, 1);
	controller->SetCmdByte(4, 0);
	EXPECT_CALL(*controller, DataOut());
	device->Dispatch(scsi_command::eCmdPowerViewWriteColorPalette);
	EXPECT_TRUE(power_view->WriteByteSequence(vector<uint8_t>(256 * 4)));

	// An offset-zero update with the CA control byte set must not be treated as
	// a 800 by 600 mode change. The initial surface remains 640 by 400.
	controller->SetCmdByte(1, 0);
	controller->SetCmdByte(2, 0);
	controller->SetCmdByte(3, 0);
	controller->SetCmdByte(4, 0x03);
	controller->SetCmdByte(5, 0x20);
	controller->SetCmdByte(6, 0x02);
	controller->SetCmdByte(7, 0x58);
	controller->SetCmdByte(9, 1);
	EXPECT_THAT([&] { device->Dispatch(scsi_command::eCmdPowerViewWriteFrameBuffer); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))));
	EXPECT_EQ(640, power_view->GetScreenWidth());
	EXPECT_EQ(400, power_view->GetScreenHeight());
}

TEST(ScsiPowerViewTest, WritesThrottledPpmSnapshot)
{
	const path snapshot = test_data_temp_path / "powerview.ppm";
	create_directories(snapshot.parent_path());
	remove(snapshot);
	remove(snapshot.string() + ".tmp");

	auto controller = make_shared<NiceMock<MockAbstractController>>(0);
	auto power_view = make_shared<SCSIPowerView>(0);
	EXPECT_TRUE(power_view->Init({ { "snapshot", snapshot.string() }, { "snapshot_interval", "60000" } }));
	EXPECT_TRUE(controller->AddDevice(power_view));

	controller->SetCmdByte(3, 0);
	controller->SetCmdByte(4, 2);
	EXPECT_CALL(*controller, DataOut());
	power_view->Dispatch(scsi_command::eCmdPowerViewWriteColorPalette);
	EXPECT_TRUE(power_view->WriteByteSequence(vector<uint8_t>{
			0x00, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00
	}));

	controller->SetCmdByte(1, 0);
	controller->SetCmdByte(2, 0);
	controller->SetCmdByte(3, 0);
	controller->SetCmdByte(4, 0);
	controller->SetCmdByte(5, 1);
	controller->SetCmdByte(6, 0);
	controller->SetCmdByte(7, 1);
	EXPECT_CALL(*controller, DataOut());
	power_view->Dispatch(scsi_command::eCmdPowerViewWriteFrameBuffer);
	EXPECT_TRUE(power_view->WriteByteSequence(vector<uint8_t>{ 0x80 }));
	for (int attempts = 0; attempts < 100 && !exists(snapshot); ++attempts) {
		this_thread::sleep_for(chrono::milliseconds(10));
	}

	ifstream input(snapshot, ios::binary);
	ASSERT_TRUE(input);
	string header;
	getline(input, header);
	EXPECT_EQ("P6", header);
	getline(input, header);
	EXPECT_EQ("640 400", header);
	getline(input, header);
	EXPECT_EQ("255", header);

	array<uint8_t, 6> pixels {};
	input.read(reinterpret_cast<char*>(pixels.data()), static_cast<streamsize>(pixels.size()));
	const array<uint8_t, 6> expected_pixels = { 0x00, 0x00, 0x00, 0xff, 0xff, 0xff };
	EXPECT_EQ(expected_pixels, pixels);
	EXPECT_FALSE(exists(snapshot.string() + ".tmp"));

	EXPECT_CALL(*controller, DataOut());
	power_view->Dispatch(scsi_command::eCmdPowerViewWriteFrameBuffer);
	EXPECT_TRUE(power_view->WriteByteSequence(vector<uint8_t>{ 0x00 }));
	EXPECT_EQ(0x00, power_view->GetPixel(0, 0));

	ifstream throttled_input(snapshot, ios::binary);
	getline(throttled_input, header);
	getline(throttled_input, header);
	getline(throttled_input, header);
	array<uint8_t, 3> throttled_pixel {};
	throttled_input.read(reinterpret_cast<char*>(throttled_pixel.data()), static_cast<streamsize>(throttled_pixel.size()));
	const array<uint8_t, 3> expected_throttled_pixel = { 0x00, 0x00, 0x00 };
	EXPECT_EQ(expected_throttled_pixel, throttled_pixel);

	remove(snapshot);
}

TEST(ScsiPowerViewTest, RejectsInvalidDataOutLengths)
{
	auto [controller, device] = CreateDevice(SCPV);

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

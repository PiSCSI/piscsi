//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "testing.h"
#include "rascsi_exceptions.h"
#include "devices/primary_device.h"
#include "devices/device_factory.h"

TEST(PrimaryDeviceTest, UnitReady)
{
	MockScsiController controller(nullptr, 0);
	MockPrimaryDevice device;

	controller.AddDevice(&device);

	controller.ctrl.cmd[0] = eCmdTestUnitReady;

	device.SetReset(true);
	device.SetAttn(true);
	device.SetReady(false);
	EXPECT_CALL(controller, DataIn()).Times(0);
	EXPECT_THROW(device.Dispatch(), scsi_error_exception);

	device.SetReset(false);
	EXPECT_CALL(controller, DataIn()).Times(0);
	EXPECT_THROW(device.Dispatch(), scsi_error_exception);

	device.SetReset(true);
	device.SetAttn(false);
	EXPECT_CALL(controller, DataIn()).Times(0);
	EXPECT_THROW(device.Dispatch(), scsi_error_exception);

	device.SetReset(false);
	device.SetAttn(true);
	EXPECT_CALL(controller, DataIn()).Times(0);
	EXPECT_THROW(device.Dispatch(), scsi_error_exception);

	device.SetAttn(false);
	EXPECT_CALL(controller, DataIn()).Times(0);
	EXPECT_THROW(device.Dispatch(), scsi_error_exception);

	device.SetReady(true);
	EXPECT_CALL(controller, Status()).Times(1);
	EXPECT_TRUE(device.Dispatch());
	EXPECT_TRUE(controller.ctrl.status == scsi_defs::status::GOOD);
}

TEST(PrimaryDeviceTest, Inquiry)
{
	MockScsiController controller(nullptr, 0);
	MockPrimaryDevice device;

	device.SetController(&controller);

	controller.ctrl.cmd[0] = eCmdInquiry;
	// ALLOCATION LENGTH
	controller.ctrl.cmd[4] = 255;

	ON_CALL(device, InquiryInternal()).WillByDefault([&device]() {
		return device.HandleInquiry(device_type::PROCESSOR, scsi_level::SPC_3, false);
	});
	EXPECT_CALL(device, InquiryInternal()).Times(1);
	EXPECT_CALL(controller, DataIn()).Times(1);
	EXPECT_TRUE(device.Dispatch());
	EXPECT_EQ(0x7F, controller.ctrl.buffer[0]) << "Invalid LUN was not reported";

	EXPECT_TRUE(controller.AddDevice(&device));
	EXPECT_FALSE(controller.AddDevice(&device)) << "Duplicate LUN was not rejected";
	EXPECT_CALL(device, InquiryInternal()).Times(1);
	EXPECT_CALL(controller, DataIn()).Times(1);
	EXPECT_TRUE(device.Dispatch());
	EXPECT_EQ(device_type::PROCESSOR, controller.ctrl.buffer[0]);
	EXPECT_EQ(0x00, controller.ctrl.buffer[1]) << "Device was not reported as non-removable";
	EXPECT_EQ(scsi_level::SPC_3, controller.ctrl.buffer[2]) << "Wrong SCSI level";
	EXPECT_EQ(scsi_level::SCSI_2, controller.ctrl.buffer[3]) << "Wrong response level";
	EXPECT_EQ(0x1F, controller.ctrl.buffer[4]) << "Wrong additional data size";

	ON_CALL(device, InquiryInternal()).WillByDefault([&device]() {
		return device.HandleInquiry(device_type::DIRECT_ACCESS, scsi_level::SCSI_1_CCS, true);
	});
	EXPECT_CALL(device, InquiryInternal()).Times(1);
	EXPECT_CALL(controller, DataIn()).Times(1);
	EXPECT_TRUE(device.Dispatch());
	EXPECT_EQ(device_type::DIRECT_ACCESS, controller.ctrl.buffer[0]);
	EXPECT_EQ(0x80, controller.ctrl.buffer[1]) << "Device was not reported as removable";
	EXPECT_EQ(scsi_level::SCSI_1_CCS, controller.ctrl.buffer[2]) << "Wrong SCSI level";
	EXPECT_EQ(scsi_level::SCSI_1_CCS, controller.ctrl.buffer[3]) << "Wrong response level";
	EXPECT_EQ(0x1F, controller.ctrl.buffer[4]) << "Wrong additional data size";

	controller.ctrl.cmd[1] = 0x01;
	EXPECT_CALL(controller, DataIn()).Times(0);
	EXPECT_THROW(device.Dispatch(), scsi_error_exception) << "EVPD bit is not supported";

	controller.ctrl.cmd[2] = 0x01;
	EXPECT_CALL(controller, DataIn()).Times(0);
	EXPECT_THROW(device.Dispatch(), scsi_error_exception) << "PAGE CODE field is not supported";

	controller.ctrl.cmd[1] = 0x00;
	controller.ctrl.cmd[2] = 0x00;
	// ALLOCATION LENGTH
	controller.ctrl.cmd[4] = 1;
	EXPECT_CALL(device, InquiryInternal()).Times(1);
	EXPECT_CALL(controller, DataIn()).Times(1);
	EXPECT_TRUE(device.Dispatch());
	EXPECT_EQ(0x1F, controller.ctrl.buffer[4]) << "Wrong additional data size";
	EXPECT_EQ(1, controller.ctrl.length) << "Wrong ALLOCATION LENGTH handling";
}

TEST(PrimaryDeviceTest, RequestSense)
{
	MockScsiController controller(nullptr, 0);
	MockPrimaryDevice device;

	controller.AddDevice(&device);

	controller.ctrl.cmd[0] = eCmdRequestSense;
	// ALLOCATION LENGTH
	controller.ctrl.cmd[4] = 255;

	device.SetReady(false);
	EXPECT_THROW(device.Dispatch(), scsi_error_exception);

	device.SetReady(true);
	EXPECT_CALL(controller, DataIn()).Times(1);
	EXPECT_TRUE(device.Dispatch());
	EXPECT_TRUE(controller.ctrl.status == scsi_defs::status::GOOD);
}

TEST(PrimaryDeviceTest, ReportLuns)
{
	const int LUN1 = 1;
	const int LUN2 = 4;

	MockScsiController controller(nullptr, 0);
	MockPrimaryDevice device1;
	MockPrimaryDevice device2;

	device1.SetLun(LUN1);
	controller.AddDevice(&device1);
	ASSERT_TRUE(controller.HasDeviceForLun(LUN1));
	device2.SetLun(LUN2);
	controller.AddDevice(&device2);
	ASSERT_TRUE(controller.HasDeviceForLun(LUN2));

	controller.ctrl.cmd[0] = eCmdReportLuns;
	// ALLOCATION LENGTH
	controller.ctrl.cmd[9] = 255;

	EXPECT_CALL(controller, DataIn()).Times(1);
	EXPECT_TRUE(device1.Dispatch());
	EXPECT_EQ(0x00, controller.ctrl.buffer[0]) << "Wrong data length";
	EXPECT_EQ(0x00, controller.ctrl.buffer[1]) << "Wrong data length";
	EXPECT_EQ(0x00, controller.ctrl.buffer[2]) << "Wrong data length";
	EXPECT_EQ(0x10, controller.ctrl.buffer[3]) << "Wrong data length";
	EXPECT_EQ(0x00, controller.ctrl.buffer[8]) << "Wrong LUN1 number";
	EXPECT_EQ(0x00, controller.ctrl.buffer[9]) << "Wrong LUN1 number";
	EXPECT_EQ(0x00, controller.ctrl.buffer[10]) << "Wrong LUN1 number";
	EXPECT_EQ(0x00, controller.ctrl.buffer[11]) << "Wrong LUN1 number";
	EXPECT_EQ(0x00, controller.ctrl.buffer[12]) << "Wrong LUN1 number";
	EXPECT_EQ(0x00, controller.ctrl.buffer[13]) << "Wrong LUN1 number";
	EXPECT_EQ(0x00, controller.ctrl.buffer[14]) << "Wrong LUN1 number";
	EXPECT_EQ(LUN1, controller.ctrl.buffer[15]) << "Wrong LUN1 number";
	EXPECT_EQ(0x00, controller.ctrl.buffer[16]) << "Wrong LUN2 number";
	EXPECT_EQ(0x00, controller.ctrl.buffer[17]) << "Wrong LUN2 number";
	EXPECT_EQ(0x00, controller.ctrl.buffer[18]) << "Wrong LUN2 number";
	EXPECT_EQ(0x00, controller.ctrl.buffer[19]) << "Wrong LUN2 number";
	EXPECT_EQ(0x00, controller.ctrl.buffer[20]) << "Wrong LUN2 number";
	EXPECT_EQ(0x00, controller.ctrl.buffer[21]) << "Wrong LUN2 number";
	EXPECT_EQ(0x00, controller.ctrl.buffer[22]) << "Wrong LUN2 number";
	EXPECT_EQ(LUN2, controller.ctrl.buffer[23]) << "Wrong LUN2 number";

	controller.ctrl.cmd[2] = 0x01;
	EXPECT_THROW(device1.Dispatch(), scsi_error_exception) << "Only SELECT REPORT mode 0 is supported";
}

TEST(PrimaryDeviceTest, UnknownCommand)
{
	MockScsiController controller(nullptr, 0);
	MockPrimaryDevice device;

	controller.AddDevice(&device);

	controller.ctrl.cmd[0] = 0xFF;
	EXPECT_FALSE(device.Dispatch());
}

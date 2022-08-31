//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "testing.h"
#include "exceptions.h"
#include "devices/primary_device.h"
#include "devices/device_factory.h"

TEST(PrimaryDeviceTest, TestUnitReady)
{
	MockScsiController controller(0);
	MockPrimaryDevice device;

	device.SetController(&controller);

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
	MockScsiController controller(0);
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

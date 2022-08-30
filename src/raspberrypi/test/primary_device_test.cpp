//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "exceptions.h"
#include "devices/primary_device.h"
#include "devices/device_factory.h"

using namespace rascsi_interface;

namespace PrimaryDeviceTest
{

class MockScsiController : public ScsiController
{
public:

	MOCK_METHOD(BUS::phase_t, Process, (int), (override));
	MOCK_METHOD(int, GetEffectiveLun, (), ());
	MOCK_METHOD(void, Error, (scsi_defs::sense_key, scsi_defs::asc, scsi_defs::status), (override));
	MOCK_METHOD(int, GetInitiatorId, (), ());
	MOCK_METHOD(void, SetUnit, (int), ());
	MOCK_METHOD(void, Connect, (int, BUS *), ());
	MOCK_METHOD(void, Status, (), ());
	MOCK_METHOD(void, DataIn, (), ());
	MOCK_METHOD(void, DataOut, (), ());
	MOCK_METHOD(void, BusFree, (), ());
	MOCK_METHOD(void, Selection, (), ());
	MOCK_METHOD(void, Command, (), ());
	MOCK_METHOD(void, MsgIn, (), ());
	MOCK_METHOD(void, MsgOut, (), ());
	MOCK_METHOD(void, Send, (), ());
	MOCK_METHOD(bool, XferMsg, (int), ());
	MOCK_METHOD(bool, XferIn, (BYTE *), ());
	MOCK_METHOD(bool, XferOut, (bool), ());
	MOCK_METHOD(void, ReceiveBytes, (), ());
	MOCK_METHOD(void, Execute, (), ());
	MOCK_METHOD(void, FlushUnit, (), ());
	MOCK_METHOD(void, Receive, (), ());
	MOCK_METHOD(bool, HasUnit, (), (const override));

	FRIEND_TEST(PrimaryDeviceTest, TestUnitReady);
	FRIEND_TEST(PrimaryDeviceTest, Inquiry);

	MockScsiController() : ScsiController(nullptr, 0) {}
	~MockScsiController() {}
};

class MockPrimaryDevice : public PrimaryDevice
{
public:

	MOCK_METHOD(vector<BYTE>, InquiryInternal, (), (const));

	MockPrimaryDevice() : PrimaryDevice("test") {}
	~MockPrimaryDevice() {}

	// Make protected methods visible for testing

	void SetReady(bool ready) { PrimaryDevice::SetReady(ready); }
	void SetReset(bool reset) { PrimaryDevice::SetReset(reset); }
	void SetAttn(bool attn) { PrimaryDevice::SetAttn(attn); }
	vector<BYTE> HandleInquiry(device_type type, scsi_level level, bool is_removable) const {
		return PrimaryDevice::HandleInquiry(type, level, is_removable);
	}
};

TEST(PrimaryDeviceTest, TestUnitReady)
{
	MockScsiController controller;
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
	MockScsiController controller;
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

	controller.SetDeviceForLun(0, &device);
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

}

//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
// Unit tests based on GoogleTest and GoogleMock
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "exceptions.h"
#include "devices/scsihd.h"
#include "devices/device_factory.h"

using namespace rascsi_interface;

namespace DeviceTest
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

	FRIEND_TEST(DeviceTest, TestUnitReady);
	FRIEND_TEST(DeviceTest, Inquiry);

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
};

DeviceFactory& device_factory = DeviceFactory::instance();

TEST(DeviceTest, TestUnitReady)
{
	MockScsiController controller;
	MockPrimaryDevice device;

	device.SetController(&controller);

	controller.ctrl.cmd[0] = eCmdTestUnitReady;

	device.SetReset(true);
	device.SetAttn(true);
	device.SetReady(false);
	EXPECT_THROW(device.Dispatch(), scsi_error_exception);

	device.SetReset(false);
	EXPECT_THROW(device.Dispatch(), scsi_error_exception);

	device.SetReset(true);
	device.SetAttn(false);
	EXPECT_THROW(device.Dispatch(), scsi_error_exception);

	device.SetReset(false);
	device.SetAttn(true);
	EXPECT_THROW(device.Dispatch(), scsi_error_exception);

	device.SetAttn(false);
	EXPECT_THROW(device.Dispatch(), scsi_error_exception);

	device.SetReady(true);
	EXPECT_CALL(controller, Status()).Times(1);
	EXPECT_TRUE(device.Dispatch());
	EXPECT_TRUE(controller.ctrl.status == scsi_defs::status::GOOD);
}

TEST(DeviceTest, Inquiry)
{
	MockScsiController controller;
	MockPrimaryDevice device;

	device.SetController(&controller);

	controller.ctrl.cmd[0] = eCmdInquiry;

	EXPECT_CALL(device, InquiryInternal()).Times(1);
	EXPECT_CALL(controller, DataIn()).Times(1);
	EXPECT_TRUE(device.Dispatch());

	// TODO Evaluate return data
}

}

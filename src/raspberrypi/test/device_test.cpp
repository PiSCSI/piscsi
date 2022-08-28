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

class MockController : public ScsiController
{
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

	MockController() : ScsiController(nullptr, 0) { }
	~MockController() { }
};

DeviceFactory& device_factory = DeviceFactory::instance();

TEST(DeviceTest, TestUnitReady)
{
	MockController controller;
	PrimaryDevice *device = static_cast<PrimaryDevice *>(device_factory.CreateDevice(SCHD, "test"));

	controller.ctrl.cmd[0] = eCmdTestUnitReady;
	device->SetController(&controller);

	EXPECT_THROW(device->Dispatch(), scsi_error_exception);

	// TODO Add tests for a device that is ready after the SASI code removal
}

}

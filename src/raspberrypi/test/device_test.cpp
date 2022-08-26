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

#include "gpiobus.h"
#include "../devices/scsihd.h"
#include "../devices/device_factory.h"

using namespace rascsi_interface;

namespace DeviceTest
{

class MockController : public SCSIDEV
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
	MOCK_METHOD(void, Send, (), (override));
	MOCK_METHOD(bool, XferMsg, (int), ());
	MOCK_METHOD(bool, XferIn, (BYTE *), ());
	MOCK_METHOD(bool, XferOut, (bool), (override));
	MOCK_METHOD(void, ReceiveBytes, (), ());
	MOCK_METHOD(void, Execute, (), (override));
	MOCK_METHOD(void, FlushUnit, (), ());
	MOCK_METHOD(void, Receive, (), (override));
	MOCK_METHOD(bool, HasUnit, (), (const override));

	FRIEND_TEST(DeviceTest, TestUnitReady);

	MockController() { }
	~MockController() { }
};

DeviceFactory& device_factory = DeviceFactory::instance();

TEST(DeviceTest, TestUnitReady)
{
	MockController controller;
	SCSIHD *device = (SCSIHD *)device_factory.CreateDevice(SCHD, "test");

	controller.ctrl.cmd[0] = eCmdTestUnitReady;
	EXPECT_TRUE(device->Dispatch(&controller));
}

}

//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "testing.h"
#include "scsi.h"
#include "rascsi_exceptions.h"
#include "controllers/abstract_controller.h"

using namespace scsi_defs;

TEST(AbstractControllerTest, Reset)
{
	MockAbstractController controller(0);
	auto device = new MockPrimaryDevice(0);

	controller.AddDevice(device);

	controller.SetPhase(BUS::phase_t::status);
	EXPECT_EQ(BUS::phase_t::status, controller.GetPhase());
	EXPECT_CALL(*device, Reset()).Times(1);
	controller.Reset();
	EXPECT_TRUE(controller.IsBusFree());
	EXPECT_EQ(status::GOOD, controller.GetStatus());
	EXPECT_EQ(0, controller.GetLength());
}

TEST(AbstractControllerTest, SetGetStatus)
{
	MockAbstractController controller(0);

	controller.SetStatus(status::BUSY);
	EXPECT_EQ(status::BUSY, controller.GetStatus());
}

TEST(AbstractControllerTest, SetPhase)
{
	MockAbstractController controller(0);

	controller.SetPhase(BUS::phase_t::selection);
	EXPECT_TRUE(controller.IsSelection());

	controller.SetPhase(BUS::phase_t::busfree);
	EXPECT_TRUE(controller.IsBusFree());

	controller.SetPhase(BUS::phase_t::command);
	EXPECT_TRUE(controller.IsCommand());

	controller.SetPhase(BUS::phase_t::status);
	EXPECT_TRUE(controller.IsStatus());

	controller.SetPhase(BUS::phase_t::datain);
	EXPECT_TRUE(controller.IsDataIn());

	controller.SetPhase(BUS::phase_t::dataout);
	EXPECT_TRUE(controller.IsDataOut());

	controller.SetPhase(BUS::phase_t::msgin);
	EXPECT_TRUE(controller.IsMsgIn());

	controller.SetPhase(BUS::phase_t::msgout);
	EXPECT_TRUE(controller.IsMsgOut());
}

TEST(AbstractControllerTest, ProcessPhase)
{
	MockAbstractController controller(0);

	controller.SetPhase(BUS::phase_t::selection);
	EXPECT_CALL(controller, Selection()).Times(1);
	controller.ProcessPhase();

	controller.SetPhase(BUS::phase_t::busfree);
	EXPECT_CALL(controller, BusFree()).Times(1);
	controller.ProcessPhase();

	controller.SetPhase(BUS::phase_t::datain);
	EXPECT_CALL(controller, DataIn()).Times(1);
	controller.ProcessPhase();

	controller.SetPhase(BUS::phase_t::dataout);
	EXPECT_CALL(controller, DataOut()).Times(1);
	controller.ProcessPhase();

	controller.SetPhase(BUS::phase_t::command);
	EXPECT_CALL(controller, Command()).Times(1);
	controller.ProcessPhase();

	controller.SetPhase(BUS::phase_t::status);
	EXPECT_CALL(controller, Status()).Times(1);
	controller.ProcessPhase();

	controller.SetPhase(BUS::phase_t::msgin);
	EXPECT_CALL(controller, MsgIn()).Times(1);
	controller.ProcessPhase();

	controller.SetPhase(BUS::phase_t::msgout);
	EXPECT_CALL(controller, MsgOut()).Times(1);
	controller.ProcessPhase();

	controller.SetPhase(BUS::phase_t::reselection);
	EXPECT_THROW(controller.ProcessPhase(), scsi_error_exception);

	controller.SetPhase(BUS::phase_t::reserved);
	EXPECT_THROW(controller.ProcessPhase(), scsi_error_exception);
}

TEST(AbstractControllerTest, DeviceLunLifeCycle)
{
	const int ID = 1;
	const int LUN = 4;

	MockAbstractController controller(ID);
	auto device1 = new MockPrimaryDevice(LUN);
	auto device2 = new MockPrimaryDevice(32);
	auto device3 = new MockPrimaryDevice(-1);

	EXPECT_EQ(0, controller.GetLunCount());
	EXPECT_EQ(ID, controller.GetTargetId());
	EXPECT_TRUE(controller.AddDevice(device1));
	EXPECT_FALSE(controller.AddDevice(device2));
	EXPECT_FALSE(controller.AddDevice(device3));
	EXPECT_TRUE(controller.GetLunCount() > 0);
	EXPECT_TRUE(controller.HasDeviceForLun(LUN));
	EXPECT_FALSE(controller.HasDeviceForLun(0));
	EXPECT_NE(nullptr, controller.GetDeviceForLun(LUN));
	EXPECT_EQ(nullptr, controller.GetDeviceForLun(0));
	EXPECT_TRUE(controller.DeleteDevice(*device1));
	EXPECT_EQ(0, controller.GetLunCount());
}

TEST(AbstractControllerTest, ExtractInitiatorId)
{
	const int ID = 1;
	const int INITIATOR_ID = 7;
	const int UNKNOWN_INITIATOR_ID = -1;

	MockAbstractController controller(ID);

	EXPECT_EQ(INITIATOR_ID, controller.ExtractInitiatorId((1 << INITIATOR_ID) | ( 1 << ID)));
	EXPECT_EQ(UNKNOWN_INITIATOR_ID, controller.ExtractInitiatorId(1 << ID));
}

TEST(AbstractControllerTest, GetOpcode)
{
	MockAbstractController controller(0);

	vector<int>& cmd = controller.InitCmd(1);

	cmd[0] = 0x12;
	EXPECT_EQ(0x12, (int)controller.GetOpcode());
}

TEST(AbstractControllerTest, GetLun)
{
	const int LUN = 3;

	MockAbstractController controller(0);

	vector<int>& cmd = controller.InitCmd(2);

	cmd[1] = LUN << 5;
	EXPECT_EQ(LUN, controller.GetLun());
}

TEST(AbstractControllerTest, Length)
{
	MockAbstractController controller(0);

	EXPECT_FALSE(controller.HasValidLength());

	controller.UpdateOffsetAndLength();
	EXPECT_EQ(0, controller.GetLength());
}

TEST(AbstractControllerTest, Offset)
{
	MockAbstractController controller(0);

	controller.ResetOffset();
	EXPECT_EQ(0, controller.GetOffset());

	controller.UpdateOffsetAndLength();
	EXPECT_EQ(0, controller.GetOffset());
}

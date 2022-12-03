//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/rascsi_exceptions.h"
#include "controllers/abstract_controller.h"

using namespace scsi_defs;

TEST(AbstractControllerTest, AllocateCmd)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	MockAbstractController controller(controller_manager, 0);

	EXPECT_EQ(16, controller.GetCmd().size());
	controller.AllocateCmd(1234);
	EXPECT_EQ(1234, controller.GetCmd().size());
}

TEST(AbstractControllerTest, AllocateBuffer)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	MockAbstractController controller(controller_manager, 0);

	controller.AllocateBuffer(1);
	EXPECT_LE(1, controller.GetBuffer().size());
	controller.AllocateBuffer(10000);
	EXPECT_LE(10000, controller.GetBuffer().size());
}

TEST(AbstractControllerTest, Reset)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto device = make_shared<MockPrimaryDevice>(0);

	controller->AddDevice(device);

	controller->SetPhase(phase_t::status);
	EXPECT_EQ(phase_t::status, controller->GetPhase());
	controller->Reset();
	EXPECT_TRUE(controller->IsBusFree());
	EXPECT_EQ(status::GOOD, controller->GetStatus());
	EXPECT_EQ(0, controller->GetLength());
}

TEST(AbstractControllerTest, Next)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	MockAbstractController controller(controller_manager, 0);

	controller.SetNext(0x1234);
	EXPECT_EQ(0x1234, controller.GetNext());
	controller.IncrementNext();
	EXPECT_EQ(0x1235, controller.GetNext());
}

TEST(AbstractControllerTest, Message)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	MockAbstractController controller(controller_manager, 0);

	controller.SetMessage(0x12);
	EXPECT_EQ(0x12, controller.GetMessage());
}

TEST(AbstractControllerTest, ByteTransfer)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	MockAbstractController controller(controller_manager, 0);

	controller.SetByteTransfer(false);
	EXPECT_FALSE(controller.IsByteTransfer());
	controller.SetByteTransfer(true);
	EXPECT_TRUE(controller.IsByteTransfer());
}

TEST(AbstractControllerTest, BytesToTransfer)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	MockAbstractController controller(controller_manager, 0);

	controller.SetBytesToTransfer(0x1234);
	EXPECT_EQ(0x1234, controller.GetBytesToTransfer());
	controller.SetByteTransfer(false);
	EXPECT_EQ(0, controller.GetBytesToTransfer());
}

TEST(AbstractControllerTest, GetMaxLuns)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	MockAbstractController controller(controller_manager, 0);

	EXPECT_EQ(32, controller.GetMaxLuns());
}

TEST(AbstractControllerTest, Status)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	MockAbstractController controller(controller_manager, 0);

	controller.SetStatus(status::RESERVATION_CONFLICT);
	EXPECT_EQ(status::RESERVATION_CONFLICT, controller.GetStatus());
}

TEST(AbstractControllerTest, ProcessPhase)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	MockAbstractController controller(controller_manager, 0);

	controller.SetPhase(phase_t::selection);
	EXPECT_CALL(controller, Selection);
	controller.ProcessPhase();

	controller.SetPhase(phase_t::busfree);
	EXPECT_CALL(controller, BusFree);
	controller.ProcessPhase();

	controller.SetPhase(phase_t::datain);
	EXPECT_CALL(controller, DataIn);
	controller.ProcessPhase();

	controller.SetPhase(phase_t::dataout);
	EXPECT_CALL(controller, DataOut);
	controller.ProcessPhase();

	controller.SetPhase(phase_t::command);
	EXPECT_CALL(controller, Command);
	controller.ProcessPhase();

	controller.SetPhase(phase_t::status);
	EXPECT_CALL(controller, Status);
	controller.ProcessPhase();

	controller.SetPhase(phase_t::msgin);
	EXPECT_CALL(controller, MsgIn);
	controller.ProcessPhase();

	controller.SetPhase(phase_t::msgout);
	EXPECT_CALL(controller, MsgOut);
	controller.ProcessPhase();

	controller.SetPhase(phase_t::reselection);
	EXPECT_THAT([&] { controller.ProcessPhase(); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::ABORTED_COMMAND),
			Property(&scsi_exception::get_asc, asc::NO_ADDITIONAL_SENSE_INFORMATION))));

	controller.SetPhase(phase_t::reserved);
	EXPECT_THAT([&] { controller.ProcessPhase(); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::ABORTED_COMMAND),
			Property(&scsi_exception::get_asc, asc::NO_ADDITIONAL_SENSE_INFORMATION))));
}

TEST(AbstractControllerTest, DeviceLunLifeCycle)
{
	const int ID = 1;
	const int LUN = 4;

	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<NiceMock<MockAbstractController>>(controller_manager, ID);

	auto device1 = make_shared<MockPrimaryDevice>(LUN);
	auto device2 = make_shared<MockPrimaryDevice>(32);
	auto device3 = make_shared<MockPrimaryDevice>(-1);

	EXPECT_EQ(0, controller->GetLunCount());
	EXPECT_EQ(ID, controller->GetTargetId());
	EXPECT_TRUE(controller->AddDevice(device1));
	EXPECT_FALSE(controller->AddDevice(device2));
	EXPECT_FALSE(controller->AddDevice(device3));
	EXPECT_TRUE(controller->GetLunCount() > 0);
	EXPECT_TRUE(controller->HasDeviceForLun(LUN));
	EXPECT_FALSE(controller->HasDeviceForLun(0));
	EXPECT_NE(nullptr, controller->GetDeviceForLun(LUN));
	EXPECT_EQ(nullptr, controller->GetDeviceForLun(0));
	EXPECT_TRUE(controller->RemoveDevice(device1));
	EXPECT_EQ(0, controller->GetLunCount());
	EXPECT_FALSE(controller->RemoveDevice(device1));
}

TEST(AbstractControllerTest, ExtractInitiatorId)
{
	const int ID = 1;
	const int INITIATOR_ID = 7;

	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	MockAbstractController controller(controller_manager, ID);

	EXPECT_EQ(INITIATOR_ID, controller.ExtractInitiatorId((1 << INITIATOR_ID) | ( 1 << ID)));
	EXPECT_EQ(AbstractController::UNKNOWN_INITIATOR_ID, controller.ExtractInitiatorId(1 << ID));
}

TEST(AbstractControllerTest, GetOpcode)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	MockAbstractController controller(controller_manager, 0);

	auto& cmd = controller.GetCmd();

	cmd[0] = static_cast<int>(scsi_command::eCmdInquiry);
	EXPECT_EQ(scsi_command::eCmdInquiry, controller.GetOpcode());
}

TEST(AbstractControllerTest, GetLun)
{
	const int LUN = 3;

	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	MockAbstractController controller(controller_manager, 0);

	auto& cmd = controller.GetCmd();

	cmd[1] = LUN << 5;
	EXPECT_EQ(LUN, controller.GetLun());
}

TEST(AbstractControllerTest, Blocks)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	MockAbstractController controller(controller_manager, 0);

	controller.SetBlocks(1);
	EXPECT_EQ(1, controller.GetBlocks());
	controller.DecrementBlocks();
	EXPECT_EQ(0, controller.GetBlocks());
}

TEST(AbstractControllerTest, Length)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	MockAbstractController controller(controller_manager, 0);

	EXPECT_FALSE(controller.HasValidLength());

	controller.SetLength(1);
	EXPECT_EQ(1, controller.GetLength());
	EXPECT_TRUE(controller.HasValidLength());
}

TEST(AbstractControllerTest, UpdateOffsetAndLength)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	MockAbstractController controller(controller_manager, 0);

	EXPECT_FALSE(controller.HasValidLength());

	controller.UpdateOffsetAndLength();
	EXPECT_EQ(0, controller.GetLength());
}

TEST(AbstractControllerTest, Offset)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	MockAbstractController controller(controller_manager, 0);

	controller.ResetOffset();
	EXPECT_EQ(0, controller.GetOffset());

	controller.UpdateOffsetAndLength();
	EXPECT_EQ(0, controller.GetOffset());
}

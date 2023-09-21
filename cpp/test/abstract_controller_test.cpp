//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/piscsi_exceptions.h"
#include "controllers/abstract_controller.h"

using namespace scsi_defs;

TEST(AbstractControllerTest, AllocateCmd)
{
	MockAbstractController controller;

	EXPECT_EQ(16, controller.GetCmd().size());
	controller.AllocateCmd(1234);
	EXPECT_EQ(1234, controller.GetCmd().size());
}

TEST(AbstractControllerTest, AllocateBuffer)
{
	MockAbstractController controller;

	controller.AllocateBuffer(1);
	EXPECT_LE(1, controller.GetBuffer().size());
	controller.AllocateBuffer(10000);
	EXPECT_LE(10000, controller.GetBuffer().size());
}

TEST(AbstractControllerTest, Reset)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(bus, 0);
	auto device = make_shared<MockPrimaryDevice>(0);

	controller->AddDevice(device);

	controller->SetPhase(phase_t::status);
	EXPECT_EQ(phase_t::status, controller->GetPhase());
	EXPECT_CALL(*bus, Reset());
	controller->Reset();
	EXPECT_TRUE(controller->IsBusFree());
	EXPECT_EQ(status::good, controller->GetStatus());
	EXPECT_EQ(0, controller->GetLength());
}

TEST(AbstractControllerTest, Next)
{
	MockAbstractController controller;

	controller.SetNext(0x1234);
	EXPECT_EQ(0x1234, controller.GetNext());
	controller.IncrementNext();
	EXPECT_EQ(0x1235, controller.GetNext());
}

TEST(AbstractControllerTest, Message)
{
	MockAbstractController controller;

	controller.SetMessage(0x12);
	EXPECT_EQ(0x12, controller.GetMessage());
}

TEST(AbstractControllerTest, ByteTransfer)
{
	MockAbstractController controller;

	controller.SetByteTransfer(false);
	EXPECT_FALSE(controller.IsByteTransfer());
	controller.SetByteTransfer(true);
	EXPECT_TRUE(controller.IsByteTransfer());
}

TEST(AbstractControllerTest, BytesToTransfer)
{
	MockAbstractController controller;

	controller.SetBytesToTransfer(0x1234);
	EXPECT_EQ(0x1234, controller.GetBytesToTransfer());
	controller.SetByteTransfer(false);
	EXPECT_EQ(0, controller.GetBytesToTransfer());
}

TEST(AbstractControllerTest, GetMaxLuns)
{
	MockAbstractController controller;

	EXPECT_EQ(32, controller.GetMaxLuns());
}

TEST(AbstractControllerTest, Status)
{
	MockAbstractController controller;

	controller.SetStatus(status::reservation_conflict);
	EXPECT_EQ(status::reservation_conflict, controller.GetStatus());
}

TEST(AbstractControllerTest, DeviceLunLifeCycle)
{
	const int ID = 1;
	const int LUN = 4;

	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<NiceMock<MockAbstractController>>(bus, ID);

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

TEST(AbstractControllerTest, GetOpcode)
{
	MockAbstractController controller;

	controller.SetCmdByte(0, static_cast<int>(scsi_command::eCmdInquiry));
	EXPECT_EQ(scsi_command::eCmdInquiry, controller.GetOpcode());
}

TEST(AbstractControllerTest, GetLun)
{
	const int LUN = 3;

	MockAbstractController controller;

	controller.SetCmdByte(1, LUN << 5);
	EXPECT_EQ(LUN, controller.GetLun());
}

TEST(AbstractControllerTest, Blocks)
{
	MockAbstractController controller;

	controller.SetBlocks(1);
	EXPECT_EQ(1, controller.GetBlocks());
	controller.DecrementBlocks();
	EXPECT_EQ(0, controller.GetBlocks());
}

TEST(AbstractControllerTest, Length)
{
	MockAbstractController controller;

	EXPECT_FALSE(controller.HasValidLength());

	controller.SetLength(1);
	EXPECT_EQ(1, controller.GetLength());
	EXPECT_TRUE(controller.HasValidLength());
}

TEST(AbstractControllerTest, UpdateOffsetAndLength)
{
	MockAbstractController controller;

	EXPECT_FALSE(controller.HasValidLength());

	controller.UpdateOffsetAndLength();
	EXPECT_EQ(0, controller.GetLength());
}

TEST(AbstractControllerTest, Offset)
{
	MockAbstractController controller;

	controller.ResetOffset();
	EXPECT_EQ(0, controller.GetOffset());

	controller.UpdateOffsetAndLength();
	EXPECT_EQ(0, controller.GetOffset());
}

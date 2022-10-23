//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "controllers/controller_manager.h"
#include "devices/scsi_printer.h"
#include "mocks.h"
#include "rascsi_exceptions.h"

using namespace std;

TEST(ScsiPrinterTest, TestUnitReady)
{
    NiceMock<MockAbstractController> controller(make_shared<MockBus>(), 0);
    auto printer = CreateDevice(SCLP, controller);

    EXPECT_CALL(controller, Status()).Times(1);
    EXPECT_TRUE(printer->Dispatch(scsi_command::eCmdTestUnitReady));
    EXPECT_EQ(status::GOOD, controller.GetStatus());
}

TEST(ScsiPrinterTest, Inquiry)
{
    TestInquiry(SCLP, device_type::PRINTER, scsi_level::SCSI_2, scsi_level::SCSI_2, "RaSCSI  SCSI PRINTER    ", 0x1f,
                false);
}

TEST(ScsiPrinterTest, ReserveUnit)
{
    NiceMock<MockAbstractController> controller(make_shared<MockBus>(), 0);
    auto printer = CreateDevice(SCLP, controller);

    EXPECT_CALL(controller, Status()).Times(1);
    EXPECT_TRUE(printer->Dispatch(scsi_command::eCmdReserve6));
    EXPECT_EQ(status::GOOD, controller.GetStatus());
}

TEST(ScsiPrinterTest, ReleaseUnit)
{
    NiceMock<MockAbstractController> controller(make_shared<MockBus>(), 0);
    auto printer = CreateDevice(SCLP, controller);

    EXPECT_CALL(controller, Status()).Times(1);
    EXPECT_TRUE(printer->Dispatch(scsi_command::eCmdRelease6));
    EXPECT_EQ(status::GOOD, controller.GetStatus());
}

TEST(ScsiPrinterTest, SendDiagnostic)
{
    NiceMock<MockAbstractController> controller(make_shared<MockBus>(), 0);
    auto printer = CreateDevice(SCLP, controller);

    EXPECT_CALL(controller, Status()).Times(1);
    EXPECT_TRUE(printer->Dispatch(scsi_command::eCmdSendDiag));
    EXPECT_EQ(status::GOOD, controller.GetStatus());
}

TEST(ScsiPrinterTest, StopPrint)
{
    NiceMock<MockAbstractController> controller(make_shared<MockBus>(), 0);
    auto printer = CreateDevice(SCLP, controller);

    EXPECT_CALL(controller, Status()).Times(1);
    EXPECT_TRUE(printer->Dispatch(scsi_command::eCmdStartStop));
    EXPECT_EQ(status::GOOD, controller.GetStatus());
}

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

TEST(ScsiPrinterTest, Init)
{
	NiceMock<MockAbstractController> controller(make_shared<MockBus>(), 0);
	auto printer = CreateDevice(SCLP, controller);

	unordered_map<string, string> params;
	EXPECT_TRUE(printer->Init(params));

	params["cmd"] = "missing_filename_specifier";
	EXPECT_FALSE(printer->Init(params));

	params["cmd"] = "%f";
	EXPECT_TRUE(printer->Init(params));
}

TEST(ScsiPrinterTest, Dispatch)
{
	TestDispatch(SCLP);
}

TEST(ScsiPrinterTest, TestUnitReady)
{
	NiceMock<MockAbstractController> controller(make_shared<MockBus>(), 0);
	auto printer = CreateDevice(SCLP, controller);

    EXPECT_CALL(controller, Status());
    EXPECT_TRUE(printer->Dispatch(scsi_command::eCmdTestUnitReady));
    EXPECT_EQ(status::GOOD, controller.GetStatus());
}

TEST(ScsiPrinterTest, Inquiry)
{
	TestInquiry(SCLP, device_type::PRINTER, scsi_level::SCSI_2,	"RaSCSI  SCSI PRINTER    ", 0x1f, false);
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

TEST(ScsiPrinterTest, Print)
{
	NiceMock<MockAbstractController> controller(make_shared<MockBus>(), 0);
	auto printer = CreateDevice(SCLP, controller);

	vector<int>& cmd = controller.GetCmd();

    EXPECT_CALL(controller, DataOut());
    EXPECT_TRUE(printer->Dispatch(scsi_command::eCmdPrint));

    cmd[3] = 0xff;
    cmd[4] = 0xff;
    EXPECT_THROW(printer->Dispatch(scsi_command::eCmdPrint), scsi_exception) << "Buffer overflow was not reported";
}

TEST(ScsiPrinterTest, StopPrint)
{
	NiceMock<MockAbstractController> controller(make_shared<MockBus>(), 0);
	auto printer = CreateDevice(SCLP, controller);

    EXPECT_CALL(controller, Status());
    EXPECT_TRUE(printer->Dispatch(scsi_command::eCmdStopPrint));
    EXPECT_EQ(status::GOOD, controller.GetStatus());
}

TEST(ScsiPrinterTest, WriteByteSequence)
{
	NiceMock<MockAbstractController> controller(make_shared<MockBus>(), 0);
	auto printer = dynamic_pointer_cast<SCSIPrinter>(CreateDevice(SCLP, controller));

	vector<BYTE> buf(1);
	EXPECT_TRUE(printer->WriteByteSequence(buf, buf.size()));
	printer->Cleanup();
}

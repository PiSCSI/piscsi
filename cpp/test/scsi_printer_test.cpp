//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/piscsi_exceptions.h"
#include "controllers/controller_manager.h"
#include "devices/scsi_printer.h"

using namespace std;

TEST(ScsiPrinterTest, Init)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<NiceMock<MockAbstractController>>(controller_manager, 0);
	auto printer = CreateDevice(SCLP, *controller);

	unordered_map<string, string> params;
	EXPECT_TRUE(printer->Init(params));

	params["cmd"] = "missing_filename_specifier";
	EXPECT_FALSE(printer->Init(params));

	params["cmd"] = "%f";
	EXPECT_TRUE(printer->Init(params));
}

TEST(ScsiPrinterTest, TestUnitReady)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<NiceMock<MockAbstractController>>(controller_manager, 0);
	auto printer = CreateDevice(SCLP, *controller);

    EXPECT_CALL(*controller, Status());
    printer->Dispatch(scsi_command::eCmdTestUnitReady);
    EXPECT_EQ(status::GOOD, controller->GetStatus());
}

TEST(ScsiPrinterTest, Inquiry)
{
	TestInquiry(SCLP, device_type::PRINTER, scsi_level::SCSI_2,	"PiSCSI  SCSI PRINTER    ", 0x1f, false);
}

TEST(ScsiPrinterTest, ReserveUnit)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<NiceMock<MockAbstractController>>(controller_manager, 0);
	auto printer = CreateDevice(SCLP, *controller);

    EXPECT_CALL(*controller, Status()).Times(1);
    printer->Dispatch(scsi_command::eCmdReserve6);
    EXPECT_EQ(status::GOOD, controller->GetStatus());
}

TEST(ScsiPrinterTest, ReleaseUnit)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<NiceMock<MockAbstractController>>(controller_manager, 0);
	auto printer = CreateDevice(SCLP, *controller);

    EXPECT_CALL(*controller, Status()).Times(1);
    printer->Dispatch(scsi_command::eCmdRelease6);
    EXPECT_EQ(status::GOOD, controller->GetStatus());
}

TEST(ScsiPrinterTest, SendDiagnostic)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<NiceMock<MockAbstractController>>(controller_manager, 0);
	auto printer = CreateDevice(SCLP, *controller);

    EXPECT_CALL(*controller, Status()).Times(1);
    printer->Dispatch(scsi_command::eCmdSendDiagnostic);
    EXPECT_EQ(status::GOOD, controller->GetStatus());
}

TEST(ScsiPrinterTest, Print)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<NiceMock<MockAbstractController>>(controller_manager, 0);
	auto printer = CreateDevice(SCLP, *controller);

	auto& cmd = controller->GetCmd();

    EXPECT_CALL(*controller, DataOut());
    printer->Dispatch(scsi_command::eCmdPrint);

    cmd[3] = 0xff;
    cmd[4] = 0xff;
    EXPECT_THAT([&] { printer->Dispatch(scsi_command::eCmdPrint); }, Throws<scsi_exception>(AllOf(
    		Property(&scsi_exception::get_sense_key, sense_key::ILLEGAL_REQUEST),
			Property(&scsi_exception::get_asc, asc::INVALID_FIELD_IN_CDB))))
    	<< "Buffer overflow was not reported";
}

TEST(ScsiPrinterTest, StopPrint)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<NiceMock<MockAbstractController>>(controller_manager, 0);
	auto printer = CreateDevice(SCLP, *controller);

    EXPECT_CALL(*controller, Status());
    printer->Dispatch(scsi_command::eCmdStopPrint);
    EXPECT_EQ(status::GOOD, controller->GetStatus());
}

TEST(ScsiPrinterTest, SynchronizeBuffer)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<NiceMock<MockAbstractController>>(controller_manager, 0);
	auto printer = CreateDevice(SCLP, *controller);

    EXPECT_THAT([&] { printer->Dispatch(scsi_command::eCmdSynchronizeBuffer); }, Throws<scsi_exception>(AllOf(
    		Property(&scsi_exception::get_sense_key, sense_key::ABORTED_COMMAND),
			Property(&scsi_exception::get_asc, asc::NO_ADDITIONAL_SENSE_INFORMATION))))
		<< "Nothing to print";

	// Further testing would use the printing system
}

TEST(ScsiPrinterTest, WriteByteSequence)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<NiceMock<MockAbstractController>>(controller_manager, 0);
	auto printer = CreateDevice(SCLP, *controller);

	vector<uint8_t> buf(1);
	EXPECT_TRUE(printer->WriteByteSequence(buf));
}

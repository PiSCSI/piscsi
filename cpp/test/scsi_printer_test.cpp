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
#include "devices/scsi_printer.h"

using namespace std;

TEST(ScsiPrinterTest, GetDefaultParams)
{
	const auto [controller, printer] = CreateDevice(SCLP);
	const auto params = printer->GetDefaultParams();
	EXPECT_EQ(1, params.size());
}

TEST(ScsiPrinterTest, Init)
{
	auto [controller, printer] = CreateDevice(SCLP);
	EXPECT_TRUE(printer->Init({}));

	param_map params;
	params["cmd"] = "missing_filename_specifier";
	EXPECT_FALSE(printer->Init(params));

	params["cmd"] = "%f";
	EXPECT_TRUE(printer->Init(params));
}

TEST(ScsiPrinterTest, TestUnitReady)
{
	auto [controller, printer] = CreateDevice(SCLP);

    EXPECT_CALL(*controller, Status());
    printer->Dispatch(scsi_command::eCmdTestUnitReady);
    EXPECT_EQ(status::good, controller->GetStatus());
}

TEST(ScsiPrinterTest, Inquiry)
{
	TestInquiry::Inquiry(SCLP, device_type::printer, scsi_level::scsi_2, "PiSCSI  SCSI PRINTER    ", 0x1f, false);
}

TEST(ScsiPrinterTest, ReserveUnit)
{
	auto [controller, printer] = CreateDevice(SCLP);

    EXPECT_CALL(*controller, Status()).Times(1);
    printer->Dispatch(scsi_command::eCmdReserve6);
    EXPECT_EQ(status::good, controller->GetStatus());
}

TEST(ScsiPrinterTest, ReleaseUnit)
{
	auto [controller, printer] = CreateDevice(SCLP);

    EXPECT_CALL(*controller, Status()).Times(1);
    printer->Dispatch(scsi_command::eCmdRelease6);
    EXPECT_EQ(status::good, controller->GetStatus());
}

TEST(ScsiPrinterTest, SendDiagnostic)
{
	auto [controller, printer] = CreateDevice(SCLP);

    EXPECT_CALL(*controller, Status()).Times(1);
    printer->Dispatch(scsi_command::eCmdSendDiagnostic);
    EXPECT_EQ(status::good, controller->GetStatus());
}

TEST(ScsiPrinterTest, Print)
{
	auto [controller, printer] = CreateDevice(SCLP);
	// Required by the bullseye clang++ compiler
	auto p = printer;

    EXPECT_CALL(*controller, DataOut());
    printer->Dispatch(scsi_command::eCmdPrint);

    controller->SetCmdByte(3, 0xff);
    controller->SetCmdByte(4, 0xff);
    EXPECT_THAT([&] { p->Dispatch(scsi_command::eCmdPrint); }, Throws<scsi_exception>(AllOf(
    		Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))))
    	<< "Buffer overflow was not reported";
}

TEST(ScsiPrinterTest, StopPrint)
{
	auto [controller, printer] = CreateDevice(SCLP);

    EXPECT_CALL(*controller, Status());
    printer->Dispatch(scsi_command::eCmdStopPrint);
    EXPECT_EQ(status::good, controller->GetStatus());
}

TEST(ScsiPrinterTest, SynchronizeBuffer)
{
	auto [controller, printer] = CreateDevice(SCLP);
	// Required by the bullseye clang++ compiler
	auto p = printer;

    EXPECT_THAT([&] { p->Dispatch(scsi_command::eCmdSynchronizeBuffer); }, Throws<scsi_exception>(AllOf(
    		Property(&scsi_exception::get_sense_key, sense_key::aborted_command),
			Property(&scsi_exception::get_asc, asc::no_additional_sense_information))))
		<< "Nothing to print";

	// Further testing would use the printing system
}

TEST(ScsiPrinterTest, WriteByteSequence)
{
	auto [controller, printer] = CreateDevice(SCLP);

	const vector<uint8_t> buf(1);
	EXPECT_TRUE(printer->WriteByteSequence(buf));
}

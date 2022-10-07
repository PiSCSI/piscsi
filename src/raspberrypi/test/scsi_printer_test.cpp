//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "rascsi_exceptions.h"
#include "controllers/controller_manager.h"
#include "devices/scsi_printer.h"

using namespace std;

TEST(ScsiPrinterTest, TestUnitReady)
{
	MockBus bus;
	ControllerManager controller_manager(bus);
	NiceMock<MockAbstractController> controller(0);
	auto device = make_shared<SCSIPrinter>(0);

    controller.AddDevice(device);

    controller.InitCmd(6);

    EXPECT_CALL(controller, DataIn()).Times(1);
    EXPECT_TRUE(device->Dispatch(scsi_command::eCmdInquiry));
}

TEST(ScsiPrinterTest, Inquiry)
{
	TestInquiry(SCLP, device_type::PRINTER, scsi_level::SCSI_2, scsi_level::SCSI_2,
			"RaSCSI  SCSI PRINTER    ", 0x1f, false);
}

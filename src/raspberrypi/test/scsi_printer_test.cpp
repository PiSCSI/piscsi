//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "rascsi_version.h"
#include "rascsi_exceptions.h"
#include "controllers/controller_manager.h"
#include "devices/scsi_printer.h"
#include <sstream>

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
	MockBus bus;
	ControllerManager controller_manager(bus);
	DeviceFactory device_factory;
    NiceMock<MockAbstractController> controller(0);
	auto device = device_factory.CreateDevice(controller_manager, SCLP, 0, "");

    controller.AddDevice(device);

    vector<int>& cmd = controller.InitCmd(6);

    // ALLOCATION LENGTH
    cmd[4] = 255;
    EXPECT_CALL(controller, DataIn()).Times(1);
    EXPECT_TRUE(device->Dispatch(scsi_command::eCmdInquiry));
	const vector<BYTE>& buffer = controller.GetBuffer();
	EXPECT_EQ((int)device_type::PRINTER, buffer[0]);
	EXPECT_EQ(0x00, buffer[1]);
	EXPECT_EQ((int)scsi_level::SCSI_2, buffer[2]);
	EXPECT_EQ((int)scsi_level::SCSI_2, buffer[3]);
	EXPECT_EQ(0x1f, buffer[4]);
	ostringstream s;
	s << "RaSCSI  SCSI PRINTER    " << setw(2) << setfill('0') << rascsi_major_version << rascsi_minor_version;
	EXPECT_TRUE(!memcmp(s.str().c_str(), &buffer[8], 28));
}

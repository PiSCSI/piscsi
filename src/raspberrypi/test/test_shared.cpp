//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "controllers/controller_manager.h"
#include "rascsi_version.h"
#include "test_shared.h"
#include <sstream>

using namespace std;

void TestInquiry(PbDeviceType type, device_type t, scsi_level l, scsi_level r, const string& ident,
		int additional_length, bool removable)
{
	MockBus bus;
	ControllerManager controller_manager(bus);
	DeviceFactory device_factory;
    NiceMock<MockAbstractController> controller(0);
	auto device = device_factory.CreateDevice(controller_manager, type, 0, "");

    controller.AddDevice(device);

    vector<int>& cmd = controller.InitCmd(6);

    // ALLOCATION LENGTH
    cmd[4] = 255;
    EXPECT_CALL(controller, DataIn()).Times(1);
    EXPECT_TRUE(device->Dispatch(scsi_command::eCmdInquiry));
	const vector<BYTE>& buffer = controller.GetBuffer();
	EXPECT_EQ((int)t, buffer[0]);
	EXPECT_EQ(removable ? 0x80: 0x00, buffer[1]);
	EXPECT_EQ((int)l, buffer[2]);
	EXPECT_EQ((int)r, buffer[3]);
	EXPECT_EQ(additional_length, buffer[4]);
	string product_data;
	if (ident.size() == 24) {
		ostringstream s;
		s << ident << setw(2) << setfill('0') << rascsi_major_version << rascsi_minor_version;
		product_data = s.str();
	}
	else {
		product_data = ident;
	}
	EXPECT_TRUE(!memcmp(product_data.c_str(), &buffer[8], 28));
}

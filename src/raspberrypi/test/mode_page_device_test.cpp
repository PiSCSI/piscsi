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
#include "devices/mode_page_device.h"

using namespace std;

TEST(ModePageDeviceTest, AddModePages)
{
	vector<int> cdb(6);
	vector<BYTE> buf(512);
	MockModePageDevice device;

	cdb[2] = 0x3f;

	EXPECT_EQ(0, device.AddModePages(cdb, buf, 0, -1)) << "Negative maximum length must be rejected";
	EXPECT_EQ(0, device.AddModePages(cdb, buf, 0, 0)) << "Allocation length 0 must be rejected";
	EXPECT_EQ(1, device.AddModePages(cdb, buf, 0, 1)) << "Allocation length 1 must be rejected";

	cdb[2] = 0x00;
	EXPECT_THROW(device.AddModePages(cdb, buf, 0, 12), scsi_error_exception)
		<< "Data were returned for non-existing mode page 0";
}

TEST(ModePageDeviceTest, ModeSense6)
{
    MockAbstractController controller(0);
	auto device = make_shared<NiceMock<MockModePageDevice>>();

    controller.AddDevice(device);

    controller.InitCmd(6);

    EXPECT_CALL(controller, DataIn()).Times(1);
    EXPECT_TRUE(device->Dispatch(scsi_command::eCmdModeSense6));
}

TEST(ModePageDeviceTest, ModeSense10)
{
    MockAbstractController controller(0);
	auto device = make_shared<NiceMock<MockModePageDevice>>();

    controller.AddDevice(device);

    controller.InitCmd(10);

    EXPECT_CALL(controller, DataIn()).Times(1);
    EXPECT_TRUE(device->Dispatch(scsi_command::eCmdModeSense10));
}

TEST(ModePageDeviceTest, ModeSelect6)
{
    MockAbstractController controller(0);
	auto device = make_shared<MockModePageDevice>();

    controller.AddDevice(device);

    vector<int>& cmd = controller.InitCmd(6);

    EXPECT_CALL(controller, DataOut()).Times(1);
    EXPECT_TRUE(device->Dispatch(scsi_command::eCmdModeSelect6));

    cmd[1] = 0x01;
    EXPECT_THROW(device->Dispatch(scsi_command::eCmdModeSelect6), scsi_error_exception)
    	<< "Saving parameters is not supported for most device types";
}

TEST(ModePageDeviceTest, ModeSelect10)
{
    MockAbstractController controller(0);
	auto device = make_shared<MockModePageDevice>();

    controller.AddDevice(device);

    vector<int>& cmd = controller.InitCmd(10);

    EXPECT_CALL(controller, DataOut()).Times(1);
    EXPECT_TRUE(device->Dispatch(scsi_command::eCmdModeSelect10));

    cmd[1] = 0x01;
    EXPECT_THROW(device->Dispatch(scsi_command::eCmdModeSelect10), scsi_error_exception)
    	<< "Saving parameters is not supported for most device types";
}

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

TEST(ModePageDeviceTest, SupportsSaveParameters)
{
	MockModePageDevice device;

	EXPECT_FALSE(device.SupportsSaveParameters()) << "Wrong default value";
	device.SupportsSaveParameters(true);
	EXPECT_TRUE(device.SupportsSaveParameters());
	device.SupportsSaveParameters(false);
	EXPECT_FALSE(device.SupportsSaveParameters());
}

TEST(ModePageDeviceTest, AddModePages)
{
	vector<int> cdb(6);
	vector<BYTE> buf(512);
	MockModePageDevice device;

	// Page 0
	cdb[2] = 0x00;
	EXPECT_THROW(device.AddModePages(cdb, buf, 0, 12, 255), scsi_exception)
		<< "Data were returned for non-existing mode page 0";

	// All pages, non changeable
	cdb[2] = 0x3f;
	EXPECT_EQ(0, device.AddModePages(cdb, buf, 0, 0, 255));
	EXPECT_EQ(3, device.AddModePages(cdb, buf, 0, 3, 255));
	EXPECT_THROW(device.AddModePages(cdb, buf, 0, 12, -1), scsi_exception) << "Maximum size was ignored";

	// All pages, changeable
	cdb[2]= 0x7f;
	EXPECT_EQ(0, device.AddModePages(cdb, buf, 0, 0, 255));
	EXPECT_EQ(3, device.AddModePages(cdb, buf, 0, 3, 255));
	EXPECT_THROW(device.AddModePages(cdb, buf, 0, 12, -1), scsi_exception) << "Maximum size was ignored";
}

TEST(ModePageDeviceTest, Page0)
{
	vector<int> cdb(6);
	vector<BYTE> buf(512);
	MockPage0ModePageDevice device;

	cdb[2] = 0x3f;
	EXPECT_EQ(0, device.AddModePages(cdb, buf, 0, 0, 255));
	EXPECT_EQ(1, device.AddModePages(cdb, buf, 0, 1, 255));
}

TEST(ModePageDeviceTest, AddVendorPage)
{
	map<int, vector<byte>> pages;
	MockModePageDevice device;

	device.AddVendorPage(pages, 0x3f, false);
	EXPECT_TRUE(pages.empty()) << "There must not be any default vendor page";
	device.AddVendorPage(pages, 0x3f, true);
	EXPECT_TRUE(pages.empty()) << "There must not be any default vendor page";
}

TEST(ModePageDeviceTest, Dispatch)
{
    MockAbstractController controller(0);
	auto device = make_shared<NiceMock<MockModePageDevice>>();

    controller.AddDevice(device);

    EXPECT_FALSE(device->Dispatch(scsi_command::eCmdIcd)) << "Command is not supported by this class";
}

TEST(ModePageDeviceTest, ModeSense6)
{
    MockAbstractController controller(0);
	auto device = make_shared<NiceMock<MockModePageDevice>>();

    controller.AddDevice(device);

    EXPECT_CALL(controller, DataIn());
    EXPECT_TRUE(device->Dispatch(scsi_command::eCmdModeSense6));
}

TEST(ModePageDeviceTest, ModeSense10)
{
    MockAbstractController controller(0);
	auto device = make_shared<NiceMock<MockModePageDevice>>();

    controller.AddDevice(device);

    EXPECT_CALL(controller, DataIn());
    EXPECT_TRUE(device->Dispatch(scsi_command::eCmdModeSense10));
}

TEST(ModePageDeviceTest, ModeSelect)
{
	MockModePageDevice device;
	vector<int> cmd;
	vector<BYTE> buf;

	EXPECT_THROW(device.ModeSelect(scsi_command::eCmdModeSelect6, cmd, buf, 0), scsi_exception)
		<< "Unexpected MODE SELECT(6) default implementation";
	EXPECT_THROW(device.ModeSelect(scsi_command::eCmdModeSelect10, cmd, buf, 0), scsi_exception)
		<< "Unexpected MODE SELECT(10) default implementation";
}

TEST(ModePageDeviceTest, ModeSelect6)
{
    MockAbstractController controller(0);
	auto device = make_shared<MockModePageDevice>();

    controller.AddDevice(device);

    vector<int>& cmd = controller.GetCmd();

    EXPECT_CALL(controller, DataOut());
    EXPECT_TRUE(device->Dispatch(scsi_command::eCmdModeSelect6));

    cmd[1] = 0x01;
    EXPECT_THROW(device->Dispatch(scsi_command::eCmdModeSelect6), scsi_exception)
    	<< "Saving parameters is not supported for most device types";
}

TEST(ModePageDeviceTest, ModeSelect10)
{
    MockAbstractController controller(0);
	auto device = make_shared<MockModePageDevice>();

    controller.AddDevice(device);

    vector<int>& cmd = controller.GetCmd();

    EXPECT_CALL(controller, DataOut());
    EXPECT_TRUE(device->Dispatch(scsi_command::eCmdModeSelect10));

    cmd[1] = 0x01;
    EXPECT_THROW(device->Dispatch(scsi_command::eCmdModeSelect10), scsi_exception)
    	<< "Saving parameters is not supported for most device types";
}

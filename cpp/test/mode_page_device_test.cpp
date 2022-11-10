//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/rascsi_exceptions.h"
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
	vector<uint8_t> buf(512);
	MockModePageDevice device;

	// Page 0
	cdb[2] = 0x00;
	EXPECT_THAT([&] { device.AddModePages(cdb, buf, 0, 12, 255); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::ILLEGAL_REQUEST),
			Property(&scsi_exception::get_asc, asc::INVALID_FIELD_IN_CDB))))
		<< "Data were returned for non-existing mode page 0";

	// All pages, non changeable
	cdb[2] = 0x3f;
	EXPECT_EQ(0, device.AddModePages(cdb, buf, 0, 0, 255));
	EXPECT_EQ(3, device.AddModePages(cdb, buf, 0, 3, 255));
	EXPECT_THAT([&] { device.AddModePages(cdb, buf, 0, 12, -1); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::ILLEGAL_REQUEST),
			Property(&scsi_exception::get_asc, asc::INVALID_FIELD_IN_CDB))))
		<< "Maximum size was ignored";

	// All pages, changeable
	cdb[2]= 0x7f;
	EXPECT_EQ(0, device.AddModePages(cdb, buf, 0, 0, 255));
	EXPECT_EQ(3, device.AddModePages(cdb, buf, 0, 3, 255));
	EXPECT_THAT([&] { device.AddModePages(cdb, buf, 0, 12, -1); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::ILLEGAL_REQUEST),
			Property(&scsi_exception::get_asc, asc::INVALID_FIELD_IN_CDB))))
		<< "Maximum size was ignored";
}

TEST(ModePageDeviceTest, Page0)
{
	vector<int> cdb(6);
	vector<uint8_t> buf(512);
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

TEST(ModePageDeviceTest, ModeSense6)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto device = make_shared<NiceMock<MockModePageDevice>>();
	const unordered_map<string, string> params;
	device->Init(params);

    controller->AddDevice(device);

    EXPECT_CALL(*controller, DataIn());
    device->Dispatch(scsi_command::eCmdModeSense6);
}

TEST(ModePageDeviceTest, ModeSense10)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto device = make_shared<NiceMock<MockModePageDevice>>();
	const unordered_map<string, string> params;
	device->Init(params);

    controller->AddDevice(device);

    EXPECT_CALL(*controller, DataIn());
    device->Dispatch(scsi_command::eCmdModeSense10);
}

TEST(ModePageDeviceTest, ModeSelect)
{
	MockModePageDevice device;
	vector<int> cmd;
	vector<uint8_t> buf;

	EXPECT_THAT([&] { device.ModeSelect(scsi_command::eCmdModeSelect6, cmd, buf, 0); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::ILLEGAL_REQUEST),
			Property(&scsi_exception::get_asc, asc::INVALID_COMMAND_OPERATION_CODE))))
		<< "Unexpected MODE SELECT(6) default implementation";
	EXPECT_THAT([&] { device.ModeSelect(scsi_command::eCmdModeSelect10, cmd, buf, 0); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::ILLEGAL_REQUEST),
			Property(&scsi_exception::get_asc, asc::INVALID_COMMAND_OPERATION_CODE))))
		<< "Unexpected MODE SELECT(10) default implementation";
}

TEST(ModePageDeviceTest, ModeSelect6)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto device = make_shared<MockModePageDevice>();
	const unordered_map<string, string> params;
	device->Init(params);

    controller->AddDevice(device);

    auto& cmd = controller->GetCmd();

    EXPECT_CALL(*controller, DataOut());
    device->Dispatch(scsi_command::eCmdModeSelect6);

    cmd[1] = 0x01;
	EXPECT_THAT([&] { device->Dispatch(scsi_command::eCmdModeSelect6); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::ILLEGAL_REQUEST),
			Property(&scsi_exception::get_asc, asc::INVALID_FIELD_IN_CDB))))
    	<< "Saving parameters is not supported by base class";
}

TEST(ModePageDeviceTest, ModeSelect10)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto device = make_shared<MockModePageDevice>();
	const unordered_map<string, string> params;
	device->Init(params);

    controller->AddDevice(device);

    auto& cmd = controller->GetCmd();

    EXPECT_CALL(*controller, DataOut());
    device->Dispatch(scsi_command::eCmdModeSelect10);

    cmd[1] = 0x01;
	EXPECT_THAT([&] { device->Dispatch(scsi_command::eCmdModeSelect10); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::ILLEGAL_REQUEST),
			Property(&scsi_exception::get_asc, asc::INVALID_FIELD_IN_CDB))))
    	<< "Saving parameters is not supported for by base class";
}

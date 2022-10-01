//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "testing.h"
#include "scsi.h"
#include "rascsi_exceptions.h"
#include "devices/primary_device.h"
#include "devices/device_factory.h"

using namespace scsi_defs;

TEST(PrimaryDeviceTest, PhaseChange)
{
	MockAbstractController controller(0);
	auto device = make_shared<MockPrimaryDevice>(0);

	controller.AddDevice(device);

	EXPECT_CALL(controller, Status()).Times(1);
	device->EnterStatusPhase();

	EXPECT_CALL(controller, DataIn()).Times(1);
	device->EnterDataInPhase();

	EXPECT_CALL(controller, DataOut()).Times(1);
	device->EnterDataOutPhase();
}

TEST(PrimaryDeviceTest, TestUnitReady)
{
	MockAbstractController controller(0);
	auto device = make_shared<MockPrimaryDevice>(0);

	controller.AddDevice(device);

	device->SetReset(true);
	device->SetAttn(true);
	device->SetReady(false);
	EXPECT_CALL(controller, DataIn()).Times(0);
	EXPECT_THROW(device->Dispatch(scsi_command::eCmdTestUnitReady), scsi_error_exception);

	device->SetReset(false);
	EXPECT_CALL(controller, DataIn()).Times(0);
	EXPECT_THROW(device->Dispatch(scsi_command::eCmdTestUnitReady), scsi_error_exception);

	device->SetReset(true);
	device->SetAttn(false);
	EXPECT_CALL(controller, DataIn()).Times(0);
	EXPECT_THROW(device->Dispatch(scsi_command::eCmdTestUnitReady), scsi_error_exception);

	device->SetReset(false);
	device->SetAttn(true);
	EXPECT_CALL(controller, DataIn()).Times(0);
	EXPECT_THROW(device->Dispatch(scsi_command::eCmdTestUnitReady), scsi_error_exception);

	device->SetAttn(false);
	EXPECT_CALL(controller, DataIn()).Times(0);
	EXPECT_THROW(device->Dispatch(scsi_command::eCmdTestUnitReady), scsi_error_exception);

	device->SetReady(true);
	EXPECT_CALL(controller, Status()).Times(1);
	EXPECT_TRUE(device->Dispatch(scsi_command::eCmdTestUnitReady));
	EXPECT_EQ(status::GOOD, controller.GetStatus());
}

TEST(PrimaryDeviceTest, Inquiry)
{
	MockAbstractController controller(0);
	auto device = make_shared<MockPrimaryDevice>(0);

	device->SetController(&controller);

	vector<int>& cmd = controller.InitCmd(6);
	// ALLOCATION LENGTH
	cmd[4] = 255;

	ON_CALL(*device, InquiryInternal()).WillByDefault([&device]() {
		return device->HandleInquiry(device_type::PROCESSOR, scsi_level::SPC_3, false);
	});
	EXPECT_CALL(*device, InquiryInternal()).Times(1);
	EXPECT_CALL(controller, DataIn()).Times(1);
	EXPECT_TRUE(device->Dispatch(scsi_command::eCmdInquiry));
	EXPECT_EQ(0x7F, controller.GetBuffer()[0]) << "Invalid LUN was not reported";

	EXPECT_TRUE(controller.AddDevice(device));
	EXPECT_FALSE(controller.AddDevice(device)) << "Duplicate LUN was not rejected";
	EXPECT_CALL(*device, InquiryInternal()).Times(1);
	EXPECT_CALL(controller, DataIn()).Times(1);
	EXPECT_TRUE(device->Dispatch(scsi_command::eCmdInquiry));
	EXPECT_EQ(device_type::PROCESSOR, (device_type)controller.GetBuffer()[0]);
	EXPECT_EQ(0x00, controller.GetBuffer()[1]) << "Device was not reported as non-removable";
	EXPECT_EQ(scsi_level::SPC_3, (scsi_level)controller.GetBuffer()[2]) << "Wrong SCSI level";
	EXPECT_EQ(scsi_level::SCSI_2, (scsi_level)controller.GetBuffer()[3]) << "Wrong response level";
	EXPECT_EQ(0x1f, controller.GetBuffer()[4]) << "Wrong additional data size";

	ON_CALL(*device, InquiryInternal()).WillByDefault([&device]() {
		return device->HandleInquiry(device_type::DIRECT_ACCESS, scsi_level::SCSI_1_CCS, true);
	});
	EXPECT_CALL(*device, InquiryInternal()).Times(1);
	EXPECT_CALL(controller, DataIn()).Times(1);
	EXPECT_TRUE(device->Dispatch(scsi_command::eCmdInquiry));
	EXPECT_EQ(device_type::DIRECT_ACCESS, (device_type)controller.GetBuffer()[0]);
	EXPECT_EQ(0x80, controller.GetBuffer()[1]) << "Device was not reported as removable";
	EXPECT_EQ(scsi_level::SCSI_1_CCS, (scsi_level)controller.GetBuffer()[2]) << "Wrong SCSI level";
	EXPECT_EQ(scsi_level::SCSI_1_CCS, (scsi_level)controller.GetBuffer()[3]) << "Wrong response level";
	EXPECT_EQ(0x1F, controller.GetBuffer()[4]) << "Wrong additional data size";

	cmd[1] = 0x01;
	EXPECT_CALL(controller, DataIn()).Times(0);
	EXPECT_THROW(device->Dispatch(scsi_command::eCmdInquiry), scsi_error_exception) << "EVPD bit is not supported";

	cmd[2] = 0x01;
	EXPECT_CALL(controller, DataIn()).Times(0);
	EXPECT_THROW(device->Dispatch(scsi_command::eCmdInquiry), scsi_error_exception) << "PAGE CODE field is not supported";

	cmd[1] = 0x00;
	cmd[2] = 0x00;
	// ALLOCATION LENGTH
	cmd[4] = 1;
	EXPECT_CALL(*device, InquiryInternal()).Times(1);
	EXPECT_CALL(controller, DataIn()).Times(1);
	EXPECT_TRUE(device->Dispatch(scsi_command::eCmdInquiry));
	EXPECT_EQ(0x1F, controller.GetBuffer()[4]) << "Wrong additional data size";
	EXPECT_EQ(1, controller.GetLength()) << "Wrong ALLOCATION LENGTH handling";
}

TEST(PrimaryDeviceTest, RequestSense)
{
	MockAbstractController controller(0);
	auto device = make_shared<MockPrimaryDevice>(0);

	controller.AddDevice(device);

	vector<int>& cmd = controller.InitCmd(6);
	// ALLOCATION LENGTH
	cmd[4] = 255;

	device->SetReady(false);
	EXPECT_THROW(device->Dispatch(scsi_command::eCmdRequestSense), scsi_error_exception);

	device->SetReady(true);
	EXPECT_CALL(controller, DataIn()).Times(1);
	EXPECT_TRUE(device->Dispatch(scsi_command::eCmdRequestSense));
	EXPECT_EQ(status::GOOD, controller.GetStatus());
}

TEST(PrimaryDeviceTest, ReportLuns)
{
	const int LUN1 = 1;
	const int LUN2 = 4;

	MockAbstractController controller(0);
	auto device1 = make_shared<MockPrimaryDevice>(LUN1);
	auto device2 = make_shared<MockPrimaryDevice>(LUN2);

	controller.AddDevice(device1);
	EXPECT_TRUE(controller.HasDeviceForLun(LUN1));
	controller.AddDevice(device2);
	EXPECT_TRUE(controller.HasDeviceForLun(LUN2));

	vector<int>& cmd = controller.InitCmd(10);
	// ALLOCATION LENGTH
	cmd[9] = 255;

	EXPECT_CALL(controller, DataIn()).Times(1);
	EXPECT_TRUE(device1->Dispatch(scsi_command::eCmdReportLuns));
	const vector<BYTE>& buffer = controller.GetBuffer();
	EXPECT_EQ(0x00, buffer[0]) << "Wrong data length";
	EXPECT_EQ(0x00, buffer[1]) << "Wrong data length";
	EXPECT_EQ(0x00, buffer[2]) << "Wrong data length";
	EXPECT_EQ(0x10, buffer[3]) << "Wrong data length";
	EXPECT_EQ(0x00, buffer[8]) << "Wrong LUN1 number";
	EXPECT_EQ(0x00, buffer[9]) << "Wrong LUN1 number";
	EXPECT_EQ(0x00, buffer[10]) << "Wrong LUN1 number";
	EXPECT_EQ(0x00, buffer[11]) << "Wrong LUN1 number";
	EXPECT_EQ(0x00, buffer[12]) << "Wrong LUN1 number";
	EXPECT_EQ(0x00, buffer[13]) << "Wrong LUN1 number";
	EXPECT_EQ(0x00, buffer[14]) << "Wrong LUN1 number";
	EXPECT_EQ(LUN1, buffer[15]) << "Wrong LUN1 number";
	EXPECT_EQ(0x00, buffer[16]) << "Wrong LUN2 number";
	EXPECT_EQ(0x00, buffer[17]) << "Wrong LUN2 number";
	EXPECT_EQ(0x00, buffer[18]) << "Wrong LUN2 number";
	EXPECT_EQ(0x00, buffer[19]) << "Wrong LUN2 number";
	EXPECT_EQ(0x00, buffer[20]) << "Wrong LUN2 number";
	EXPECT_EQ(0x00, buffer[21]) << "Wrong LUN2 number";
	EXPECT_EQ(0x00, buffer[22]) << "Wrong LUN2 number";
	EXPECT_EQ(LUN2, buffer[23]) << "Wrong LUN2 number";

	cmd[2] = 0x01;
	EXPECT_THROW(device1->Dispatch(scsi_command::eCmdReportLuns), scsi_error_exception) << "Only SELECT REPORT mode 0 is supported";
}

TEST(PrimaryDeviceTest, UnknownCommand)
{
	MockAbstractController controller(0);
	auto device = make_shared<MockPrimaryDevice>(0);

	controller.AddDevice(device);

	controller.InitCmd(1);
	EXPECT_FALSE(device->Dispatch((scsi_command)0xFF));
}

TEST(PrimaryDeviceTest, WriteByteSequence)
{
	vector<BYTE> data;
	MockPrimaryDevice device(0);

	EXPECT_FALSE(device.WriteByteSequence(data, 0)) << "Primary device must not support writing byte sequences";
}

TEST(PrimaryDeviceTest, GetSendDelay)
{
	MockPrimaryDevice device(0);

	EXPECT_EQ(-1, device.GetSendDelay());
}

TEST(PrimaryDeviceTest, Init)
{
	unordered_map<string, string> params;
	MockPrimaryDevice device(0);

	EXPECT_TRUE(device.Init(params)) << "Initialization of primary device must not fail";
}

TEST(PrimaryDeviceTest, FlushCache)
{
	MockPrimaryDevice device(0);

	// Method must be present
	device.FlushCache();
}

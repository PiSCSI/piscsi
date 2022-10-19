//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "scsi.h"
#include "rascsi_exceptions.h"
#include "devices/primary_device.h"
#include "devices/device_factory.h"

using namespace scsi_defs;

TEST(PrimaryDeviceTest, GetId)
{
	const int ID = 5;

	MockAbstractController controller(ID);
	auto device = make_shared<MockPrimaryDevice>(0);

	EXPECT_EQ(-1, device->GetId()) << "Device ID cannot be known without assignment to a controller";

	controller.AddDevice(device);
	EXPECT_EQ(ID, device->GetId());
}

TEST(PrimaryDeviceTest, PhaseChange)
{
	MockAbstractController controller(0);
	auto device = make_shared<MockPrimaryDevice>(0);

	controller.AddDevice(device);

	EXPECT_CALL(controller, Status());
	device->EnterStatusPhase();

	EXPECT_CALL(controller, DataIn());
	device->EnterDataInPhase();

	EXPECT_CALL(controller, DataOut());
	device->EnterDataOutPhase();
}

TEST(PrimaryDeviceTest, Reset)
{
	NiceMock<MockAbstractController> controller(0);
	auto device = make_shared<MockPrimaryDevice>(0);

	controller.AddDevice(device);

	EXPECT_TRUE(device->Dispatch(scsi_command::eCmdReserve6));
	EXPECT_FALSE(device->CheckReservation(1, scsi_command::eCmdTestUnitReady, false))
		<< "Device must be reserved for initiator ID 1";
	device->Reset();
	EXPECT_TRUE(device->CheckReservation(1, scsi_command::eCmdTestUnitReady, false))
		<< "Device must not be reserved anymore for initiator ID 1";
}

TEST(PrimaryDeviceTest, CheckReservation)
{
	NiceMock<MockAbstractController> controller(0);
	auto device = make_shared<MockPrimaryDevice>(0);

	controller.AddDevice(device);

	EXPECT_TRUE(device->CheckReservation(0, scsi_command::eCmdTestUnitReady, false))
		<< "Device must not be reserved for initiator ID 0";

	EXPECT_TRUE(device->Dispatch(scsi_command::eCmdReserve6));
	EXPECT_TRUE(device->CheckReservation(0, scsi_command::eCmdTestUnitReady, false))
		<< "Device must not be reserved for initiator ID 0";
	EXPECT_FALSE(device->CheckReservation(1, scsi_command::eCmdTestUnitReady, false))
		<< "Device must be reserved for initiator ID 1";
	EXPECT_FALSE(device->CheckReservation(-1, scsi_command::eCmdTestUnitReady, false))
		<< "Device must be reserved for unknown initiator";
	EXPECT_TRUE(device->CheckReservation(1, scsi_command::eCmdInquiry, false))
		<< "Device must not be reserved for INQUIRY";
	EXPECT_TRUE(device->CheckReservation(1, scsi_command::eCmdRequestSense, false))
		<< "Device must not be reserved for REQUEST SENSE";
	EXPECT_TRUE(device->CheckReservation(1, scsi_command::eCmdRelease6, false))
		<< "Device must not be reserved for RELEASE (6)";

	EXPECT_TRUE(device->CheckReservation(1, scsi_command::eCmdRemoval, false))
		<< "Device must not be reserved for PREVENT ALLOW MEDIUM REMOVAL with prevent bit not set";
	EXPECT_FALSE(device->CheckReservation(1, scsi_command::eCmdRemoval, true))
		<< "Device must be reserved for PREVENT ALLOW MEDIUM REMOVAL with prevent bit set";
}

TEST(PrimaryDeviceTest, ReserveReleaseUnit)
{
	NiceMock<MockAbstractController> controller(0);
	auto device = make_shared<MockPrimaryDevice>(0);

	controller.AddDevice(device);

	EXPECT_TRUE(device->Dispatch(scsi_command::eCmdReserve6));
	EXPECT_FALSE(device->CheckReservation(1, scsi_command::eCmdTestUnitReady, false))
		<< "Device must be reserved for initiator ID 1";

	EXPECT_TRUE(device->Dispatch(scsi_command::eCmdRelease6));
	EXPECT_TRUE(device->CheckReservation(1, scsi_command::eCmdTestUnitReady, false))
		<< "Device must not be reserved anymore for initiator ID 1";

	ON_CALL(controller, GetInitiatorId).WillByDefault(Return(-1));
	EXPECT_TRUE(device->Dispatch(scsi_command::eCmdReserve6));
	EXPECT_FALSE(device->CheckReservation(1, scsi_command::eCmdTestUnitReady, false))
		<< "Device must be reserved for unknown initiator";

	EXPECT_TRUE(device->Dispatch(scsi_command::eCmdRelease6));
	EXPECT_TRUE(device->CheckReservation(1, scsi_command::eCmdTestUnitReady, false))
		<< "Device must not be reserved anymore for unknown initiator";
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
	EXPECT_THROW(device->Dispatch(scsi_command::eCmdTestUnitReady), scsi_exception);

	device->SetReset(false);
	EXPECT_CALL(controller, DataIn()).Times(0);
	EXPECT_THROW(device->Dispatch(scsi_command::eCmdTestUnitReady), scsi_exception);

	device->SetReset(true);
	device->SetAttn(false);
	EXPECT_CALL(controller, DataIn()).Times(0);
	EXPECT_THROW(device->Dispatch(scsi_command::eCmdTestUnitReady), scsi_exception);

	device->SetReset(false);
	device->SetAttn(true);
	EXPECT_CALL(controller, DataIn()).Times(0);
	EXPECT_THROW(device->Dispatch(scsi_command::eCmdTestUnitReady), scsi_exception);

	device->SetAttn(false);
	EXPECT_CALL(controller, DataIn()).Times(0);
	EXPECT_THROW(device->Dispatch(scsi_command::eCmdTestUnitReady), scsi_exception);

	device->SetReady(true);
	EXPECT_CALL(controller, Status());
	EXPECT_TRUE(device->Dispatch(scsi_command::eCmdTestUnitReady));
	EXPECT_EQ(status::GOOD, controller.GetStatus());
}

TEST(PrimaryDeviceTest, Inquiry)
{
	auto controller = make_shared<NiceMock<MockAbstractController>>(0);
	auto device = make_shared<MockPrimaryDevice>(0);

	controller->AddDevice(device);

	vector<int>& cmd = controller->GetCmd();
	// ALLOCATION LENGTH
	cmd[4] = 255;

	ON_CALL(*device, InquiryInternal()).WillByDefault([&device]() {
		return device->HandleInquiry(device_type::PROCESSOR, scsi_level::SPC_3, false);
	});
	EXPECT_CALL(*device, InquiryInternal());
	EXPECT_CALL(*controller, DataIn());
	ON_CALL(*controller, GetEffectiveLun()).WillByDefault(Return(1));
	EXPECT_TRUE(device->Dispatch(scsi_command::eCmdInquiry));
	EXPECT_EQ(0x7f, controller->GetBuffer()[0]) << "Invalid LUN was not reported";
	ON_CALL(*controller, GetEffectiveLun()).WillByDefault(Return(0));

	EXPECT_FALSE(controller->AddDevice(make_shared<MockPrimaryDevice>(0))) << "Duplicate LUN was not rejected";
	EXPECT_CALL(*device, InquiryInternal());
	EXPECT_CALL(*controller, DataIn());
	EXPECT_TRUE(device->Dispatch(scsi_command::eCmdInquiry));
	EXPECT_EQ(device_type::PROCESSOR, (device_type)controller->GetBuffer()[0]);
	EXPECT_EQ(0x00, controller->GetBuffer()[1]) << "Device was not reported as non-removable";
	EXPECT_EQ(scsi_level::SPC_3, (scsi_level)controller->GetBuffer()[2]) << "Wrong SCSI level";
	EXPECT_EQ(scsi_level::SCSI_2, (scsi_level)controller->GetBuffer()[3]) << "Wrong response level";
	EXPECT_EQ(0x1f, controller->GetBuffer()[4]) << "Wrong additional data size";

	ON_CALL(*device, InquiryInternal()).WillByDefault([&device]() {
		return device->HandleInquiry(device_type::DIRECT_ACCESS, scsi_level::SCSI_1_CCS, true);
	});
	EXPECT_CALL(*device, InquiryInternal());
	EXPECT_CALL(*controller, DataIn());
	EXPECT_TRUE(device->Dispatch(scsi_command::eCmdInquiry));
	EXPECT_EQ(device_type::DIRECT_ACCESS, (device_type)controller->GetBuffer()[0]);
	EXPECT_EQ(0x80, controller->GetBuffer()[1]) << "Device was not reported as removable";
	EXPECT_EQ(scsi_level::SCSI_1_CCS, (scsi_level)controller->GetBuffer()[2]) << "Wrong SCSI level";
	EXPECT_EQ(scsi_level::SCSI_1_CCS, (scsi_level)controller->GetBuffer()[3]) << "Wrong response level";
	EXPECT_EQ(0x1f, controller->GetBuffer()[4]) << "Wrong additional data size";

	cmd[1] = 0x01;
	EXPECT_CALL(*controller, DataIn()).Times(0);
	EXPECT_THROW(device->Dispatch(scsi_command::eCmdInquiry), scsi_exception) << "EVPD bit is not supported";

	cmd[2] = 0x01;
	EXPECT_CALL(*controller, DataIn()).Times(0);
	EXPECT_THROW(device->Dispatch(scsi_command::eCmdInquiry), scsi_exception) << "PAGE CODE field is not supported";

	cmd[1] = 0x00;
	cmd[2] = 0x00;
	// ALLOCATION LENGTH
	cmd[4] = 1;
	EXPECT_CALL(*device, InquiryInternal());
	EXPECT_CALL(*controller, DataIn());
	EXPECT_TRUE(device->Dispatch(scsi_command::eCmdInquiry));
	EXPECT_EQ(0x1f, controller->GetBuffer()[4]) << "Wrong additional data size";
	EXPECT_EQ(1, controller->GetLength()) << "Wrong ALLOCATION LENGTH handling";
}

TEST(PrimaryDeviceTest, RequestSense)
{
	NiceMock<MockAbstractController> controller(0);
	auto device = make_shared<MockPrimaryDevice>(0);

	controller.AddDevice(device);

	vector<int>& cmd = controller.GetCmd();
	// ALLOCATION LENGTH
	cmd[4] = 255;

	device->SetReady(false);
	EXPECT_THROW(device->Dispatch(scsi_command::eCmdRequestSense), scsi_exception);

	device->SetReady(true);
	EXPECT_CALL(controller, DataIn());
	EXPECT_TRUE(device->Dispatch(scsi_command::eCmdRequestSense));
	EXPECT_EQ(status::GOOD, controller.GetStatus());
}

TEST(PrimaryDeviceTest, SendDiagnostic)
{
	MockAbstractController controller(0);
	auto device = make_shared<MockPrimaryDevice>(0);

	controller.AddDevice(device);

	vector<int>& cmd = controller.GetCmd();

	EXPECT_CALL(controller, Status());
	EXPECT_TRUE(device->Dispatch(scsi_command::eCmdSendDiag));
	EXPECT_EQ(status::GOOD, controller.GetStatus());

	cmd[1] = 0x10;
	EXPECT_THROW(device->Dispatch(scsi_command::eCmdSendDiag), scsi_exception)
		<< "SEND DIAGNOSTIC must fail because PF bit is not supported";
	cmd[1] = 0;

	cmd[3] = 1;
	EXPECT_THROW(device->Dispatch(scsi_command::eCmdSendDiag), scsi_exception)
		<< "SEND DIAGNOSTIC must fail because parameter list is not supported";
	cmd[3] = 0;
	cmd[4] = 1;
	EXPECT_THROW(device->Dispatch(scsi_command::eCmdSendDiag), scsi_exception)
		<< "SEND DIAGNOSTIC must fail because parameter list is not supported";
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

	vector<int>& cmd = controller.GetCmd();
	// ALLOCATION LENGTH
	cmd[9] = 255;

	EXPECT_CALL(controller, DataIn());
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
	EXPECT_THROW(device1->Dispatch(scsi_command::eCmdReportLuns), scsi_exception) << "Only SELECT REPORT mode 0 is supported";
}

TEST(PrimaryDeviceTest, UnknownCommand)
{
	MockAbstractController controller(0);
	auto device = make_shared<MockPrimaryDevice>(0);

	controller.AddDevice(device);

	EXPECT_FALSE(device->Dispatch((scsi_command)0xFF));
}

TEST(PrimaryDeviceTest, Dispatch)
{
    MockAbstractController controller(0);
	auto device = make_shared<NiceMock<MockPrimaryDevice>>(0);

    controller.AddDevice(device);

    EXPECT_FALSE(device->Dispatch(scsi_command::eCmdIcd)) << "Command is not supported by this class";
}

TEST(PrimaryDeviceTest, WriteByteSequence)
{
	vector<BYTE> data;
	MockPrimaryDevice device(0);

	EXPECT_FALSE(device.WriteByteSequence(data, 0)) << "Primary device does not support writing byte sequences";
}

TEST(PrimaryDeviceTest, GetSetSendDelay)
{
	MockPrimaryDevice device(0);

	EXPECT_EQ(-1, device.GetSendDelay()) << "Wrong default value";
	device.SetSendDelay(1234);
	EXPECT_EQ(1234, device.GetSendDelay());
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

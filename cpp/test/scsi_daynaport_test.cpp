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
#include "devices/scsi_daynaport.h"

TEST(ScsiDaynaportTest, Inquiry)
{
	TestInquiry(SCDP, device_type::processor, scsi_level::scsi_2, "Dayna   SCSI/Link       1.4a", 0x20, false);
}

TEST(ScsiDaynaportTest, TestUnitReady)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto daynaport = CreateDevice(SCDP, *controller);

    EXPECT_CALL(*controller, Status());
    daynaport->Dispatch(scsi_command::eCmdTestUnitReady);
    EXPECT_EQ(status::good, controller->GetStatus());
}

TEST(ScsiDaynaportTest, Read)
{
	vector<uint8_t> buf(0);
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto daynaport = dynamic_pointer_cast<SCSIDaynaPort>(CreateDevice(SCDP, *controller));

	auto& cmd = controller->GetCmd();

	// ALLOCATION LENGTH
	cmd[4] = 1;
	EXPECT_EQ(0, daynaport->Read(cmd, buf, 0)) << "Trying to read the root sector must fail";
}

TEST(ScsiDaynaportTest, WriteBytes)
{
	vector<uint8_t> buf(0);
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto daynaport = dynamic_pointer_cast<SCSIDaynaPort>(CreateDevice(SCDP, *controller));

	auto& cmd = controller->GetCmd();

	// Unknown data format
	cmd[5] = 0xff;
	EXPECT_TRUE(daynaport->WriteBytes(cmd, buf));
}

TEST(ScsiDaynaportTest, Read6)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto daynaport = CreateDevice(SCDP, *controller);

	auto& cmd = controller->GetCmd();

	cmd[5] = 0xff;
    EXPECT_THAT([&] { daynaport->Dispatch(scsi_command::eCmdRead6); }, Throws<scsi_exception>(AllOf(
    		Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))))
		<< "Invalid data format";
}

TEST(ScsiDaynaportTest, Write6)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto daynaport = CreateDevice(SCDP, *controller);

	auto& cmd = controller->GetCmd();

	cmd[5] = 0x00;
    EXPECT_THAT([&] { daynaport->Dispatch(scsi_command::eCmdWrite6); }, Throws<scsi_exception>(AllOf(
    		Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))))
		<< "Invalid transfer length";

	cmd[3] = -1;
	cmd[4] = -8;
	cmd[5] = 0x80;
    EXPECT_THAT([&] { daynaport->Dispatch(scsi_command::eCmdWrite6); }, Throws<scsi_exception>(AllOf(
    		Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))))
		<< "Invalid transfer length";

	cmd[3] = 0;
	cmd[4] = 0;
	cmd[5] = 0xff;
    EXPECT_THAT([&] { daynaport->Dispatch(scsi_command::eCmdWrite6); }, Throws<scsi_exception>(AllOf(
    		Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))))
		<< "Invalid transfer length";
}

TEST(ScsiDaynaportTest, TestRetrieveStats)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto daynaport = CreateDevice(SCDP, *controller);

	auto& cmd = controller->GetCmd();

    // ALLOCATION LENGTH
    cmd[4] = 255;
    EXPECT_CALL(*controller, DataIn());
	daynaport->Dispatch(scsi_command::eCmdRetrieveStats);
}

TEST(ScsiDaynaportTest, SetInterfaceMode)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto daynaport = CreateDevice(SCDP, *controller);

	auto& cmd = controller->GetCmd();

	// Unknown interface command
    EXPECT_THAT([&] { daynaport->Dispatch(scsi_command::eCmdSetIfaceMode); }, Throws<scsi_exception>(AllOf(
    		Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::invalid_command_operation_code))));

	// Not implemented, do nothing
	cmd[5] = SCSIDaynaPort::CMD_SCSILINK_SETMODE;
	EXPECT_CALL(*controller, Status());
	daynaport->Dispatch(scsi_command::eCmdSetIfaceMode);
	EXPECT_EQ(status::good, controller->GetStatus());

	cmd[5] = SCSIDaynaPort::CMD_SCSILINK_SETMAC;
	EXPECT_CALL(*controller, DataOut());
	daynaport->Dispatch(scsi_command::eCmdSetIfaceMode);

	// Not implemented
	cmd[5] = SCSIDaynaPort::CMD_SCSILINK_STATS;
	EXPECT_THAT([&] { daynaport->Dispatch(scsi_command::eCmdSetIfaceMode); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::invalid_command_operation_code))));

	// Not implemented
	cmd[5] = SCSIDaynaPort::CMD_SCSILINK_ENABLE;
	EXPECT_THAT([&] { daynaport->Dispatch(scsi_command::eCmdSetIfaceMode); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::invalid_command_operation_code))));

	// Not implemented
	cmd[5] = SCSIDaynaPort::CMD_SCSILINK_SET;
	EXPECT_THAT([&] { daynaport->Dispatch(scsi_command::eCmdSetIfaceMode); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::invalid_command_operation_code))));
}

TEST(ScsiDaynaportTest, SetMcastAddr)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto daynaport = CreateDevice(SCDP, *controller);

	auto& cmd = controller->GetCmd();

	EXPECT_THAT([&] { daynaport->Dispatch(scsi_command::eCmdSetMcastAddr); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))))
		<< "Length of 0 is not supported";

	cmd[4] = 1;
    EXPECT_CALL(*controller, DataOut());
	daynaport->Dispatch(scsi_command::eCmdSetMcastAddr);
}

TEST(ScsiDaynaportTest, EnableInterface)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto daynaport = CreateDevice(SCDP, *controller);

	auto& cmd = controller->GetCmd();

	// Enable
	EXPECT_THAT([&] { daynaport->Dispatch(scsi_command::eCmdEnableInterface); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::aborted_command),
			Property(&scsi_exception::get_asc, asc::no_additional_sense_information))));

	// Disable
	cmd[5] = 0x80;
	EXPECT_THAT([&] { daynaport->Dispatch(scsi_command::eCmdEnableInterface); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::aborted_command),
			Property(&scsi_exception::get_asc, asc::no_additional_sense_information))));
}

TEST(ScsiDaynaportTest, GetSendDelay)
{
	SCSIDaynaPort daynaport(0);
	const unordered_map<string, string> params;
	daynaport.Init(params);

	EXPECT_EQ(6, daynaport.GetSendDelay());
}

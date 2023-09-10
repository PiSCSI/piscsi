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
	TestInquiry::Inquiry(SCDP, device_type::processor, scsi_level::scsi_2, "Dayna   SCSI/Link       1.4a", 0x20, false);
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

	// ALLOCATION LENGTH
	controller->SetCmdByte(4, 1);
	EXPECT_EQ(0, daynaport->Read(controller->GetCmd(), buf, 0)) << "Trying to read the root sector must fail";
}

TEST(ScsiDaynaportTest, Write)
{
	vector<uint8_t> buf(0);
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto daynaport = dynamic_pointer_cast<SCSIDaynaPort>(CreateDevice(SCDP, *controller));

	// Unknown data format
	controller->SetCmdByte(5, 0xff);
	EXPECT_TRUE(daynaport->Write(controller->GetCmd(), buf));
}

TEST(ScsiDaynaportTest, Read6)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto daynaport = CreateDevice(SCDP, *controller);

	controller->SetCmdByte(5, 0xff);
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

	controller->SetCmdByte(5, 0x00);
    EXPECT_THAT([&] { daynaport->Dispatch(scsi_command::eCmdWrite6); }, Throws<scsi_exception>(AllOf(
    		Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))))
		<< "Invalid transfer length";

	controller->SetCmdByte(3, -1);
	controller->SetCmdByte(4, -8);
	controller->SetCmdByte(5, 0x08);
    EXPECT_THAT([&] { daynaport->Dispatch(scsi_command::eCmdWrite6); }, Throws<scsi_exception>(AllOf(
    		Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))))
		<< "Invalid transfer length";

	controller->SetCmdByte(3, 0);
	controller->SetCmdByte(4, 0);
	controller->SetCmdByte(5, 0xff);
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

    // ALLOCATION LENGTH
	controller->SetCmdByte(4, 255);
    EXPECT_CALL(*controller, DataIn());
	daynaport->Dispatch(scsi_command::eCmdRetrieveStats);
}

TEST(ScsiDaynaportTest, SetInterfaceMode)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto daynaport = CreateDevice(SCDP, *controller);

	// Unknown interface command
    EXPECT_THAT([&] { daynaport->Dispatch(scsi_command::eCmdSetIfaceMode); }, Throws<scsi_exception>(AllOf(
    		Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::invalid_command_operation_code))));

	// Not implemented, do nothing
	controller->SetCmdByte(5, SCSIDaynaPort::CMD_SCSILINK_SETMODE);
	EXPECT_CALL(*controller, Status());
	daynaport->Dispatch(scsi_command::eCmdSetIfaceMode);
	EXPECT_EQ(status::good, controller->GetStatus());

	controller->SetCmdByte(5, SCSIDaynaPort::CMD_SCSILINK_SETMAC);
	EXPECT_CALL(*controller, DataOut());
	daynaport->Dispatch(scsi_command::eCmdSetIfaceMode);

	// Not implemented
	controller->SetCmdByte(5, SCSIDaynaPort::CMD_SCSILINK_STATS);
	EXPECT_THAT([&] { daynaport->Dispatch(scsi_command::eCmdSetIfaceMode); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::invalid_command_operation_code))));

	// Not implemented
	controller->SetCmdByte(5, SCSIDaynaPort::CMD_SCSILINK_ENABLE);
	EXPECT_THAT([&] { daynaport->Dispatch(scsi_command::eCmdSetIfaceMode); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::invalid_command_operation_code))));

	// Not implemented
	controller->SetCmdByte(5, SCSIDaynaPort::CMD_SCSILINK_SET);
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

	EXPECT_THAT([&] { daynaport->Dispatch(scsi_command::eCmdSetMcastAddr); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))))
		<< "Length of 0 is not supported";

	controller->SetCmdByte(4, 1);
    EXPECT_CALL(*controller, DataOut());
	daynaport->Dispatch(scsi_command::eCmdSetMcastAddr);
}

TEST(ScsiDaynaportTest, EnableInterface)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto daynaport = CreateDevice(SCDP, *controller);

	// Enable
	EXPECT_THAT([&] { daynaport->Dispatch(scsi_command::eCmdEnableInterface); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::aborted_command),
			Property(&scsi_exception::get_asc, asc::no_additional_sense_information))));

	// Disable
	controller->SetCmdByte(5, 0x00);
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

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
#include "devices/host_services.h"
#include <array>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/json_util.h>

using namespace std;

void HostServices_SetUpModePages(map<int, vector<byte>>& pages)
{
	EXPECT_EQ(1, pages.size()) << "Unexpected number of mode pages";
	EXPECT_EQ(10, pages[32].size());
}

TEST(HostServicesTest, TestUnitReady)
{
	auto [controller, services] = CreateDevice(SCHS);

    EXPECT_CALL(*controller, Status());
    services->Dispatch(scsi_command::eCmdTestUnitReady);
    EXPECT_EQ(status::good, controller->GetStatus());
}

TEST(HostServicesTest, Inquiry)
{
	TestInquiry::Inquiry(SCHS, device_type::processor, scsi_level::spc_3, "PiSCSI  Host Services   ", 0x1f, false);
}

TEST(HostServicesTest, StartStopUnit)
{
	auto [controller, services] = CreateDevice(SCHS);
	// Required by the bullseye clang++ compiler
	auto s = services;

    // STOP
    EXPECT_CALL(*controller, Status());
    services->Dispatch(scsi_command::eCmdStartStop);
    EXPECT_EQ(status::good, controller->GetStatus());

    // LOAD
	controller->SetCmdByte(4, 0x02);
    EXPECT_CALL(*controller, Status());
    services->Dispatch(scsi_command::eCmdStartStop);
    EXPECT_EQ(status::good, controller->GetStatus());

    // UNLOAD
	controller->SetCmdByte(4, 0x03);
    EXPECT_CALL(*controller, Status());
    services->Dispatch(scsi_command::eCmdStartStop);
    EXPECT_EQ(status::good, controller->GetStatus());

    // START
	controller->SetCmdByte(4, 0x01);
	EXPECT_THAT([&] { s->Dispatch(scsi_command::eCmdStartStop); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))));
}

TEST(HostServicesTest, ModeSense6)
{
	auto [controller, services] = CreateDevice(SCHS);
	// Required by the bullseye clang++ compiler
	auto s = services;

	EXPECT_TRUE(services->Init({}));

	EXPECT_THAT([&] { s->Dispatch(scsi_command::eCmdModeSense6); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))))
    	<< "Unsupported mode page was returned";

	controller->SetCmdByte(2, 0x20);
	EXPECT_THAT([&] { s->Dispatch(scsi_command::eCmdModeSense6); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))))
    	<< "Block descriptors are not supported";

	controller->SetCmdByte(1, 0x08);
    // ALLOCATION LENGTH
	controller->SetCmdByte(4, 255);
    EXPECT_CALL(*controller, DataIn());
    services->Dispatch(scsi_command::eCmdModeSense6);
	vector<uint8_t>& buffer = controller->GetBuffer();
	// Major version 1
	EXPECT_EQ(0x01, buffer[6]);
	// Minor version 0
	EXPECT_EQ(0x00, buffer[7]);
	// Year
	EXPECT_NE(0x00, buffer[8]);
	// Day
	EXPECT_NE(0x00, buffer[10]);

    // ALLOCATION LENGTH
	controller->SetCmdByte(4, 2);
    EXPECT_CALL(*controller, DataIn());
    services->Dispatch(scsi_command::eCmdModeSense6);
	buffer = controller->GetBuffer();
	EXPECT_EQ(0x01, buffer[0]);

	controller->SetCmdByte(3, 1);
	EXPECT_THAT([&] { s->Dispatch(scsi_command::eCmdModeSense6); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))));
}

TEST(HostServicesTest, ModeSense10)
{
	auto [controller, services] = CreateDevice(SCHS);
	// Required by the bullseye clang++ compiler
	auto s = services;
	
	EXPECT_TRUE(services->Init({}));

	EXPECT_THAT([&] { s->Dispatch(scsi_command::eCmdModeSense10); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))))
    	<< "Unsupported mode page was returned";

	controller->SetCmdByte(2, 0x20);
	EXPECT_THAT([&] { s->Dispatch(scsi_command::eCmdModeSense10); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))))
    	<< "Block descriptors are not supported";

	controller->SetCmdByte(1, 0x08);
    // ALLOCATION LENGTH
	controller->SetCmdByte(8, 255);
    EXPECT_CALL(*controller, DataIn());
    services->Dispatch(scsi_command::eCmdModeSense10);
	vector<uint8_t>& buffer = controller->GetBuffer();
	// Major version 1
	EXPECT_EQ(0x01, buffer[10]);
	// Minor version 0
	EXPECT_EQ(0x00, buffer[11]);
	// Year
	EXPECT_NE(0x00, buffer[12]);
	// Day
	EXPECT_NE(0x00, buffer[14]);

    // ALLOCATION LENGTH (the two-byte header must fit in the response)
	controller->SetCmdByte(8, 4);
    EXPECT_CALL(*controller, DataIn());
    services->Dispatch(scsi_command::eCmdModeSense10);
	buffer = controller->GetBuffer();
	EXPECT_EQ(0x02, buffer[1]);

	controller->SetCmdByte(3, 1);
	EXPECT_THAT([&] { s->Dispatch(scsi_command::eCmdModeSense10); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
			Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))));
}

TEST(HostServicesTest, RemoteCommandExecution)
{
	auto [controller, services] = CreateDevice(SCHS);
	EXPECT_TRUE(services->Init({}));

	PbCommand command;
	(*command.mutable_params())["locale"] = "en";
	(*command.mutable_params())["payload"] = string(5000, 'x');
	const string data = command.SerializeAsString();

	controller->SetCmdByte(0, static_cast<int>(scsi_command::eCmdExecuteOperation));
	controller->SetCmdByte(1, 0x01);
	controller->SetCmdByte(7, static_cast<int>(data.size() >> 8));
	controller->SetCmdByte(8, static_cast<int>(data.size()));
	EXPECT_CALL(*controller, DataOut());
	services->Dispatch(scsi_command::eCmdExecuteOperation);

	auto host_services = dynamic_pointer_cast<HostServices>(services);
	host_services->SetCommandExecutor([] (const PbCommand& received, PbResult& result) {
		EXPECT_EQ("en", received.params().at("locale"));
		EXPECT_EQ(5000, received.params().at("payload").size());
		result.set_status(true);
		result.set_msg("executed");
		return true;
	});
	EXPECT_TRUE(host_services->WriteByteSequence(span(reinterpret_cast<const uint8_t*>(data.data()), data.size())));
	EXPECT_LE(data.size(), controller->GetBuffer().size());

	controller->SetCmdByte(0, static_cast<int>(scsi_command::eCmdReceiveOperationResults));
	controller->SetCmdByte(1, 0x01);
	controller->SetCmdByte(7, 0x04);
	controller->SetCmdByte(8, 0x00);
	EXPECT_CALL(*controller, DataIn());
	services->Dispatch(scsi_command::eCmdReceiveOperationResults);

	PbResult result;
	EXPECT_TRUE(result.ParseFromArray(controller->GetBuffer().data(), controller->GetLength()));
	EXPECT_TRUE(result.status());
	EXPECT_EQ("executed", result.msg());

	EXPECT_THAT([&] { services->Dispatch(scsi_command::eCmdReceiveOperationResults); }, Throws<scsi_exception>(AllOf(
		Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
		Property(&scsi_exception::get_asc, asc::data_currently_unavailable))));
}

TEST(HostServicesTest, RemoteCommandRejectsInvalidFormat)
{
	auto [controller, services] = CreateDevice(SCHS);
	EXPECT_TRUE(services->Init({}));

	controller->SetCmdByte(1, 0x03);
	controller->SetCmdByte(8, 1);
	EXPECT_THAT([&] { services->Dispatch(scsi_command::eCmdExecuteOperation); }, Throws<scsi_exception>(AllOf(
		Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
		Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))));
}

TEST(HostServicesTest, RemoteCommandRejectsMalformedPayload)
{
	auto [controller, services] = CreateDevice(SCHS);
	EXPECT_TRUE(services->Init({}));

	controller->SetCmdByte(0, static_cast<int>(scsi_command::eCmdExecuteOperation));
	controller->SetCmdByte(1, 0x01);
	controller->SetCmdByte(8, 1);
	EXPECT_CALL(*controller, DataOut());
	services->Dispatch(scsi_command::eCmdExecuteOperation);

	const array<uint8_t, 1> payload = { 0xff };
	EXPECT_THAT([&] { services->WriteByteSequence(payload); }, Throws<scsi_exception>(AllOf(
		Property(&scsi_exception::get_sense_key, sense_key::aborted_command),
		Property(&scsi_exception::get_asc, asc::internal_target_failure))));
}

TEST(HostServicesTest, RemoteCommandTextFormats)
{
	auto [controller, services] = CreateDevice(SCHS);
	EXPECT_TRUE(services->Init({}));

	PbCommand command;
	(*command.mutable_params())["locale"] = "en";
	string data;
	ASSERT_TRUE(google::protobuf::util::MessageToJsonString(command, &data).ok());

	controller->SetCmdByte(0, static_cast<int>(scsi_command::eCmdExecuteOperation));
	controller->SetCmdByte(1, 0x02);
	controller->SetCmdByte(7, static_cast<int>(data.size() >> 8));
	controller->SetCmdByte(8, static_cast<int>(data.size()));
	EXPECT_CALL(*controller, DataOut());
	services->Dispatch(scsi_command::eCmdExecuteOperation);

	auto host_services = dynamic_pointer_cast<HostServices>(services);
	host_services->SetCommandExecutor([] (const PbCommand& received, PbResult& result) {
		EXPECT_EQ("en", received.params().at("locale"));
		result.set_status(true);
		result.set_msg("text result");
		return true;
	});
	EXPECT_TRUE(host_services->WriteByteSequence(span(reinterpret_cast<const uint8_t*>(data.data()), data.size())));

	controller->SetCmdByte(0, static_cast<int>(scsi_command::eCmdReceiveOperationResults));
	controller->SetCmdByte(1, 0x04);
	controller->SetCmdByte(7, 0x04);
	controller->SetCmdByte(8, 0x00);
	EXPECT_CALL(*controller, DataIn());
	services->Dispatch(scsi_command::eCmdReceiveOperationResults);

	PbResult result;
	EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(
		string(reinterpret_cast<const char*>(controller->GetBuffer().data()), controller->GetLength()), &result));
	EXPECT_TRUE(result.status());
	EXPECT_EQ("text result", result.msg());
}

TEST(HostServicesTest, SetUpModePages)
{
	MockHostServices services(0);
	map<int, vector<byte>> pages;

	// Non changeable
	services.SetUpModePages(pages, 0x3f, false);
	HostServices_SetUpModePages(pages);

	// Changeable
	pages.clear();
	services.SetUpModePages(pages, 0x3f, true);
	HostServices_SetUpModePages(pages);
}

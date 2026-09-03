//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2026 Uwe Seimet
//
// Host Services with realtime clock, shutdown and remote command support
//
//---------------------------------------------------------------------------

//
// Features of the host services device:
//
// 1. Vendor-specific mode page 0x20 returns the current date and time, see mode_page_datetime
//
// 2. START/STOP UNIT shuts down PiSCSI or shuts down/reboots the Raspberry Pi
//   a) !start && !load (STOP): Shut down PiSCSI
//   b) !start && load (EJECT): Shut down the Raspberry Pi
//   c) start && load (LOAD): Reboot the Raspberry Pi
//
// 3. Vendor-specific remote command execution:
//   a) EXECUTE OPERATION (0xc0) receives a PbCommand
//   b) RECEIVE OPERATION RESULTS (0xc1) returns the matching PbResult
//
// Byte 1 selects exactly one protobuf encoding: binary (bit 0), JSON (bit 1)
// or text format (bit 2). Bytes 7-8 contain the transfer/allocation length.
// Results are retained per initiator until read or replaced by another command.
//

#include "shared/piscsi_exceptions.h"
#include "controllers/scsi_controller.h"
#include "scsi_command_util.h"
#include "host_services.h"
#include <algorithm>
#include <chrono>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/json_util.h>

using namespace std::chrono;
using namespace scsi_defs;
using namespace scsi_command_util;
using namespace google::protobuf;
using namespace google::protobuf::util;

bool HostServices::Init(const param_map& params)
{
	ModePageDevice::Init(params);

	AddCommand(scsi_command::eCmdTestUnitReady, [this] { TestUnitReady(); });
	AddCommand(scsi_command::eCmdStartStop, [this] { StartStopUnit(); });
	AddCommand(scsi_command::eCmdExecuteOperation, [this] { ExecuteOperation(); });
	AddCommand(scsi_command::eCmdReceiveOperationResults, [this] { ReceiveOperationResults(); });

	SetReady(true);

	return true;
}

void HostServices::TestUnitReady()
{
	// Always successful
	EnterStatusPhase();
}

vector<uint8_t> HostServices::InquiryInternal() const
{
	return HandleInquiry(device_type::processor, scsi_level::spc_3, false);
}

void HostServices::StartStopUnit() const
{
	const bool start = GetController()->GetCmdByte(4) & 0x01;
	const bool load = GetController()->GetCmdByte(4) & 0x02;

	if (!start) {
		if (load) {
			GetController()->ScheduleShutdown(AbstractController::piscsi_shutdown_mode::STOP_PI);
		}
		else {
			GetController()->ScheduleShutdown(AbstractController::piscsi_shutdown_mode::STOP_PISCSI);
		}
	}
	else if (load) {
		GetController()->ScheduleShutdown(AbstractController::piscsi_shutdown_mode::RESTART_PI);
	}
	else {
		throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
	}

	EnterStatusPhase();
}

int HostServices::GetTransferLength() const
{
	return GetInt16(GetController()->GetCmd(), 7);
}

void HostServices::ExecuteOperation()
{
	execution_results.erase(GetController()->GetInitiatorId());
	input_format = ConvertFormat();

	const int length = GetTransferLength();
	if (!length) {
		throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
	}

	// The controller's default buffer is 4 KiB; this protocol permits 64 KiB.
	GetController()->AllocateBuffer(length);
	GetController()->SetLength(length);
	GetController()->SetByteTransfer(true);
	EnterDataOutPhase();
}

void HostServices::ReceiveOperationResults()
{
	const protobuf_format output_format = ConvertFormat();
	const auto it = execution_results.find(GetController()->GetInitiatorId());
	if (it == execution_results.end()) {
		throw scsi_exception(sense_key::illegal_request, asc::data_currently_unavailable);
	}

	string data;
	switch (output_format) {
		case protobuf_format::binary:
			data = it->second;
			break;

		case protobuf_format::json: {
			if (PbResult result; !result.ParseFromString(it->second) || !MessageToJsonString(result, &data).ok()) {
				throw scsi_exception(sense_key::aborted_command, asc::internal_target_failure);
			}
			break;
		}

		case protobuf_format::text: {
			if (PbResult result; !result.ParseFromString(it->second) || !TextFormat::PrintToString(result, &data)) {
				throw scsi_exception(sense_key::aborted_command, asc::internal_target_failure);
			}
			break;
		}
	}

	execution_results.erase(it);
	const int length = min(GetTransferLength(), static_cast<int>(data.size()));
	GetController()->AllocateBuffer(length);
	memcpy(GetController()->GetBuffer().data(), data.data(), length);
	GetController()->SetLength(length);
	EnterDataInPhase();
}

bool HostServices::ParseCommand(span<const uint8_t> buf, PbCommand& command) const
{
	const int length = GetTransferLength();
	if (length <= 0 || static_cast<size_t>(length) > buf.size()) {
		return false;
	}

	switch (input_format) {
		case protobuf_format::binary:
			return command.ParseFromArray(buf.data(), length);

		case protobuf_format::json:
			return JsonStringToMessage(string(reinterpret_cast<const char*>(buf.data()), length), &command).ok();

		case protobuf_format::text:
			return TextFormat::ParseFromString(string(reinterpret_cast<const char*>(buf.data()), length), &command);
	}

	return false;
}

bool HostServices::WriteByteSequence(span<const uint8_t> buf)
{
	if (GetController()->GetCmdByte(0) != static_cast<int>(scsi_command::eCmdExecuteOperation)) {
		throw scsi_exception(sense_key::aborted_command, asc::internal_target_failure);
	}

	PbCommand command;
	if (!ParseCommand(buf, command)) {
		LogTrace("Failed to deserialize Host Services command");
		throw scsi_exception(sense_key::aborted_command, asc::internal_target_failure);
	}

	if (!execute_command) {
		LogError("Host Services command executor is not configured");
		throw scsi_exception(sense_key::aborted_command, asc::internal_target_failure);
	}

	PbResult result;
	if (!execute_command(command, result)) {
		LogTrace("Failed to execute " + PbOperation_Name(command.operation()) + " operation");
		throw scsi_exception(sense_key::aborted_command, asc::internal_target_failure);
	}

	execution_results[GetController()->GetInitiatorId()] = result.SerializeAsString();
	return true;
}

HostServices::protobuf_format HostServices::ConvertFormat() const
{
	switch (GetController()->GetCmdByte(1) & 0x1f) {
		case static_cast<int>(protobuf_format::binary): return protobuf_format::binary;
		case static_cast<int>(protobuf_format::json): return protobuf_format::json;
		case static_cast<int>(protobuf_format::text): return protobuf_format::text;
		default: throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
	}
}

int HostServices::ModeSense6(cdb_t cdb, vector<uint8_t>& buf) const
{
	// Block descriptors and subpages are not supported.
	if (cdb[3] || !(cdb[1] & 0x08)) {
		throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
	}

	const auto length = static_cast<int>(min(buf.size(), static_cast<size_t>(cdb[4])));
	fill_n(buf.begin(), length, 0);

	// 4 bytes basic information
	const int size = AddModePages(cdb, buf, 4, length, 255);

	// The mode data length does not count its own byte.
	buf[0] = static_cast<uint8_t>(size - 1);

	return size;
}

int HostServices::ModeSense10(cdb_t cdb, vector<uint8_t>& buf) const
{
	// Block descriptors and subpages are not supported.
	if (cdb[3] || !(cdb[1] & 0x08)) {
		throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
	}

	const auto length = static_cast<int>(min(buf.size(), static_cast<size_t>(GetInt16(cdb, 7))));
	fill_n(buf.begin(), length, 0);

	// 8 bytes basic information
	const int size = AddModePages(cdb, buf, 8, length, 65535);

	// The mode data length does not count its own two bytes.
	SetInt16(buf, 0, size - 2);

	return size;
}

void HostServices::SetUpModePages(map<int, vector<byte>>& pages, int page, bool changeable) const
{
	if (page == 0x20 || page == 0x3f) {
		AddRealtimeClockPage(pages, changeable);
	}
}

void HostServices::AddRealtimeClockPage(map<int, vector<byte>>& pages, bool changeable) const
{
	pages[32] = vector<byte>(10);

	if (!changeable) {
		const auto now = system_clock::now();
		const time_t t = system_clock::to_time_t(now);
		tm localtime;
		localtime_r(&t, &localtime);

		mode_page_datetime datetime;
		datetime.major_version = 0x01;
		datetime.minor_version = 0x00;
		datetime.year = (uint8_t)localtime.tm_year;
		datetime.month = (uint8_t)localtime.tm_mon;
		datetime.day = (uint8_t)localtime.tm_mday;
		datetime.hour = (uint8_t)localtime.tm_hour;
		datetime.minute = (uint8_t)localtime.tm_min;
		// Ignore leap second for simplicity
		datetime.second = (uint8_t)(localtime.tm_sec < 60 ? localtime.tm_sec : 59);

		memcpy(&pages[32][2], &datetime, sizeof(datetime));
	}
}

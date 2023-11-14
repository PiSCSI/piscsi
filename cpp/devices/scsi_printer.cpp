//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
// Implementation of a SCSI printer (see SCSI-2 specification for a command description)
//
//---------------------------------------------------------------------------

//
// How to print:
//
// 1. The client sends the data to be printed with one or several PRINT commands. The maximum
// transfer size per PRINT command is currently limited to 4096 bytes.
// 2. The client triggers printing with SYNCHRONIZE BUFFER. Each SYNCHRONIZE BUFFER results in
// the print command for this printer (see below) to be called for the data not yet printed.
//
// It is recommended to reserve the printer device before printing and to release it afterwards.
// The command to be used for printing can be set with the "cmd" property when attaching the device.
// By default the data to be printed are sent to the printer unmodified, using "lp -oraw %f". This
// requires that the client uses a printer driver compatible with the respective printer, or that the
// printing service on the Pi is configured to do any necessary conversions, or that the print command
// applies any conversions on the file to be printed (%f) before passing it to the printing service.
// 'enscript' is an example for a conversion tool.
// By attaching different devices/LUNs multiple printers (i.e. different print commands) are possible.
//
// With STOP PRINT printing can be cancelled before SYNCHRONIZE BUFFER was sent.
//

#include "shared/piscsi_exceptions.h"
#include "scsi_command_util.h"
#include "scsi_printer.h"
#include <filesystem>

using namespace std;
using namespace filesystem;
using namespace scsi_defs;
using namespace scsi_command_util;

SCSIPrinter::SCSIPrinter(int lun) : PrimaryDevice(SCLP, lun)
{
	SupportsParams(true);
}

bool SCSIPrinter::Init(const param_map& params)
{
	PrimaryDevice::Init(params);

	AddCommand(scsi_command::eCmdTestUnitReady, [this] { TestUnitReady(); });
	AddCommand(scsi_command::eCmdPrint, [this] { Print(); });
	AddCommand(scsi_command::eCmdSynchronizeBuffer, [this] { SynchronizeBuffer(); });
	// STOP PRINT is identical with TEST UNIT READY, it just returns the status
	AddCommand(scsi_command::eCmdStopPrint, [this] { TestUnitReady(); });

	// Required also in this class in order to fulfill the ScsiPrinterCommands interface contract
	AddCommand(scsi_command::eCmdReserve6, [this] { ReserveUnit(); });
	AddCommand(scsi_command::eCmdRelease6, [this] { ReleaseUnit(); });
	AddCommand(scsi_command::eCmdSendDiagnostic, [this] { SendDiagnostic(); });

	if (GetParam("cmd").find("%f") == string::npos) {
		LogTrace("Missing filename specifier %f");
		return false;
	}

	error_code error;
	file_template = temp_directory_path(error); //NOSONAR Publicly writable directory is fine here
	file_template += PRINTER_FILE_PATTERN;

	SetReady(true);

	return true;
}

void SCSIPrinter::CleanUp()
{
	PrimaryDevice::CleanUp();

	if (out.is_open()) {
		out.close();

		error_code error;
		remove(path(filename), error);

		filename = "";
	}
}

param_map SCSIPrinter::GetDefaultParams() const
{
	return {
		{ "cmd", "lp -oraw %f" }
	};
}

void SCSIPrinter::TestUnitReady()
{
	// The printer is always ready
	EnterStatusPhase();
}

vector<uint8_t> SCSIPrinter::InquiryInternal() const
{
	return HandleInquiry(device_type::printer, scsi_level::scsi_2, false);
}

void SCSIPrinter::Print()
{
	const uint32_t length = GetInt24(GetController()->GetCmd(), 2);

	LogTrace("Expecting to receive " + to_string(length) + " byte(s) to be printed");

	if (length > GetController()->GetBuffer().size()) {
		LogError("Transfer buffer overflow: Buffer size is " + to_string(GetController()->GetBuffer().size()) +
				" bytes, " + to_string(length) + " bytes expected");

		++print_error_count;

		throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
	}

	GetController()->SetLength(length);
	GetController()->SetByteTransfer(true);

	EnterDataOutPhase();
}

void SCSIPrinter::SynchronizeBuffer()
{
	if (!out.is_open()) {
		LogWarn("Nothing to print");

		++print_warning_count;

		throw scsi_exception(sense_key::aborted_command);
	}

	string cmd = GetParam("cmd");
	const size_t file_position = cmd.find("%f");
	assert(file_position != string::npos);
	cmd.replace(file_position, 2, filename);

	error_code error;
	LogTrace("Printing file '" + filename + "' with " + to_string(file_size(path(filename), error)) + " byte(s)");

	LogDebug("Executing print command '" + cmd + "'");

	if (system(cmd.c_str())) {
		LogError("Printing file '" + filename + "' failed, the printing system might not be configured");

		++print_error_count;

		CleanUp();

		throw scsi_exception(sense_key::aborted_command);
	}

	CleanUp();

	EnterStatusPhase();
}

bool SCSIPrinter::WriteByteSequence(span<const uint8_t> buf)
{
	byte_receive_count += buf.size();

	if (!out.is_open()) {
		vector<char> f(file_template.begin(), file_template.end());
		f.push_back(0);

		// There is no C++ API that generates a file with a unique name
		const int fd = mkstemp(f.data());
		if (fd == -1) {
			LogError("Can't create printer output file for pattern '" + filename + "': " + strerror(errno));

			++print_error_count;

			return false;
		}
		close(fd);

		filename = f.data();

		out.open(filename, ios::binary);
		if (out.fail()) {
			++print_error_count;

			throw scsi_exception(sense_key::aborted_command);
		}

		LogTrace("Created printer output file '" + filename + "'");
	}

	LogTrace("Appending " + to_string(buf.size()) + " byte(s) to printer output file ''" + filename + "'");

	out.write((const char *)buf.data(), buf.size());

	const bool status = out.fail();
	if (status) {
		++print_error_count;
	}

	return !status;
}

vector<PbStatistics> SCSIPrinter::GetStatistics() const
{
	vector<PbStatistics> statistics = PrimaryDevice::GetStatistics();

	PbStatistics s;
	s.set_id(GetId());
	s.set_unit(GetLun());

	s.set_category(PbStatisticsCategory::CATEGORY_INFO);

	s.set_key(FILE_PRINT_COUNT);
	s.set_value(file_print_count);
	statistics.push_back(s);

	s.set_key(BYTE_RECEIVE_COUNT);
	s.set_value(byte_receive_count);
	statistics.push_back(s);

	s.set_category(PbStatisticsCategory::CATEGORY_ERROR);

	s.set_key(PRINT_ERROR_COUNT);
	s.set_value(print_error_count);
	statistics.push_back(s);

	s.set_category(PbStatisticsCategory::CATEGORY_WARNING);

	s.set_key(PRINT_WARNING_COUNT);
	s.set_value(print_warning_count);
	statistics.push_back(s);

	return statistics;
}

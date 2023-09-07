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

bool SCSIPrinter::Init(const unordered_map<string, string>& params)
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
		GetLogger().Trace("Missing filename specifier %f");
		return false;
	}

	error_code error;
	file_template = temp_directory_path(error); //NOSONAR Publicly writable directory is fine here
	file_template += PRINTER_FILE_PATTERN;

	SetReady(true);

	return true;
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

	GetLogger().Trace("Expecting to receive " + to_string(length) + " byte(s) to be printed");

	if (length > GetController()->GetBuffer().size()) {
		GetLogger().Error("Transfer buffer overflow: Buffer size is " + to_string(GetController()->GetBuffer().size()) +
				" bytes, " + to_string(length) + " bytes expected");

		throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
	}

	GetController()->SetLength(length);
	GetController()->SetByteTransfer(true);

	EnterDataOutPhase();
}

void SCSIPrinter::SynchronizeBuffer()
{
	if (!out.is_open()) {
		GetLogger().Warn("Nothing to print");

		throw scsi_exception(sense_key::aborted_command);
	}

	string cmd = GetParam("cmd");
	const size_t file_position = cmd.find("%f");
	assert(file_position != string::npos);
	cmd.replace(file_position, 2, filename);

	error_code error;
	GetLogger().Trace("Printing file '" + filename + "' with " + to_string(file_size(path(filename), error)) + " byte(s)");

	GetLogger().Debug("Executing print command '" + cmd + "'");

	if (system(cmd.c_str())) {
		GetLogger().Error("Printing file '" + filename + "' failed, the printing system might not be configured");

		Cleanup();

		throw scsi_exception(sense_key::aborted_command);
	}

	Cleanup();

	EnterStatusPhase();
}

bool SCSIPrinter::WriteByteSequence(const span<uint8_t>& buf)
{
	if (!out.is_open()) {
		vector<char> f(file_template.begin(), file_template.end());
		f.push_back(0);

		// There is no C++ API that generates a file with a unique name
		const int fd = mkstemp(f.data());
		if (fd == -1) {
			GetLogger().Error("Can't create printer output file for pattern '" + filename + "': " + strerror(errno));
			return false;
		}
		close(fd);

		filename = f.data();

		out.open(filename, ios::binary);
		if (out.fail()) {
			throw scsi_exception(sense_key::aborted_command);
		}

		GetLogger().Trace("Created printer output file '" + filename + "'");
	}

	GetLogger().Trace("Appending " + to_string(buf.size()) + " byte(s) to printer output file ''" + filename + "'");

	out.write(reinterpret_cast<const char *>(buf.data()), buf.size());

	return !out.fail();
}

void SCSIPrinter::Cleanup()
{
	if (out.is_open()) {
		out.close();

		error_code error;
		remove(path(filename), error);

		filename = "";
	}
}

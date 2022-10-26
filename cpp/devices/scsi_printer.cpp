//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
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

#include <sys/stat.h>
#include "rascsi_exceptions.h"
#include "scsi_command_util.h"
#include "dispatcher.h"
#include "scsi_printer.h"
#include <filesystem>

using namespace std;
using namespace filesystem;
using namespace scsi_defs;
using namespace scsi_command_util;

SCSIPrinter::SCSIPrinter(int lun) : PrimaryDevice(SCLP, lun)
{
	dispatcher.Add(scsi_command::eCmdTestUnitReady, "TestUnitReady", &SCSIPrinter::TestUnitReady);
	dispatcher.Add(scsi_command::eCmdPrint, "Print", &SCSIPrinter::Print);
	dispatcher.Add(scsi_command::eCmdSynchronizeBuffer, "SynchronizeBuffer", &SCSIPrinter::SynchronizeBuffer);
	// STOP PRINT is identical with TEST UNIT READY, it just returns the status
	dispatcher.Add(scsi_command::eCmdStopPrint, "StopPrint", &SCSIPrinter::TestUnitReady);

	// Required also in this class in order to fulfill the ScsiPrinterCommands interface contract
	dispatcher.Add(scsi_command::eCmdReserve6, "ReserveUnit", &SCSIPrinter::ReserveUnit);
	dispatcher.Add(scsi_command::eCmdRelease6, "ReleaseUnit", &SCSIPrinter::ReleaseUnit);
	dispatcher.Add(scsi_command::eCmdSendDiag, "SendDiagnostic", &SCSIPrinter::SendDiagnostic);

	error_code error;
	file_template = temp_directory_path(error); //NOSONAR Publicly writable directory is fine here
	file_template += PRINTER_FILE_PATTERN;

	SupportsParams(true);
	SetReady(true);
}

SCSIPrinter::~SCSIPrinter()
{
	Cleanup();
}

bool SCSIPrinter::Init(const unordered_map<string, string>& params)
{
	SetParams(params);

	if (GetParam("cmd").find("%f") == string::npos) {
		LOGERROR("Missing filename specifier %s", "%f")
		return false;
	}

	return true;
}

bool SCSIPrinter::Dispatch(scsi_command cmd)
{
	// The superclass class handles the less specific commands
	return dispatcher.Dispatch(this, cmd) ? true : super::Dispatch(cmd);
}

void SCSIPrinter::TestUnitReady()
{
	// The printer is always ready
	EnterStatusPhase();
}

vector<byte> SCSIPrinter::InquiryInternal() const
{
	return HandleInquiry(device_type::PRINTER, scsi_level::SCSI_2, false);
}

void SCSIPrinter::Print()
{
	const uint32_t length = GetInt24(ctrl->cmd, 2);

	LOGTRACE("Receiving %d byte(s) to be printed", length)

	if (length > controller->GetBuffer().size()) {
		LOGERROR("%s", ("Transfer buffer overflow: Buffer size is " + to_string(controller->GetBuffer().size()) +
				" bytes, " + to_string(length) + " bytes expected").c_str())

		throw scsi_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	ctrl->length = length;
	controller->SetByteTransfer(true);

	EnterDataOutPhase();
}

void SCSIPrinter::SynchronizeBuffer()
{
	if (!out.is_open()) {
		LOGWARN("Nothing to print")

		throw scsi_exception(sense_key::ABORTED_COMMAND);
	}

	string cmd = GetParam("cmd");
	const size_t file_position = cmd.find("%f");
	assert(file_position != string::npos);
	cmd.replace(file_position, 2, filename);

	error_code error;

	LOGTRACE("Printing file '%s' with %s byte(s)", filename.c_str(), to_string(file_size(path(filename), error)).c_str())

	LOGDEBUG("Executing '%s'", cmd.c_str())

	if (system(cmd.c_str())) {
		LOGERROR("Printing file '%s' failed, the printing system might not be configured", filename.c_str())

		Cleanup();

		throw scsi_exception(sense_key::ABORTED_COMMAND);
	}

	Cleanup();

	EnterStatusPhase();
}

bool SCSIPrinter::WriteByteSequence(vector<BYTE>& buf, uint32_t length)
{
	if (!out.is_open()) {
		vector<char> f(file_template.begin(), file_template.end());
		f.push_back(0);

		// There is no C++ API that generates a file with a unique name
		const int fd = mkstemp(f.data());
		if (fd == -1) {
			LOGERROR("Can't create printer output file for pattern '%s': %s", filename.c_str(), strerror(errno))
			return false;
		}
		close(fd);

		filename = f.data();

		out.open(filename, ios::binary);
		if (out.fail()) {
			throw scsi_exception(sense_key::ABORTED_COMMAND);
		}

		LOGTRACE("Created printer output file '%s'", filename.c_str())
	}

	LOGTRACE("Appending %d byte(s) to printer output file '%s'", length, filename.c_str())

	out.write((const char*)buf.data(), length);

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

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
// 1. The client reserves the printer device with RESERVE UNIT (optional step, mandatory for
// a multi-initiator environment).
// 2. The client sends the data to be printed with one or several PRINT commands. Due to
// https://github.com/akuker/RASCSI/issues/669 the maximum transfer size per PRINT command is
// limited to 4096 bytes.
// 3. The client triggers printing with SYNCHRONIZE BUFFER. Each SYNCHRONIZE BUFFER results in
// the print command for this printer (see below) to be called for the data not yet printed.
// 4. The client releases the printer with RELEASE UNIT (optional step, mandatory for a
// multi-initiator environment).
//
// A client usually does not know whether it is running in a multi-initiator environment. This is why
// always using a reservation is recommended.
//
// The command to be used for printing can be set with the "cmd" property when attaching the device.
// By default the data to be printed are sent to the printer unmodified, using "lp -oraw %f". This
// requires that the client uses a printer driver compatible with the respective printer, or that the
// printing service on the Pi is configured to do any necessary conversions, or that the print command
// applies any conversions on the file to be printed (%f) before passing it to the printing service.
// 'enscript' is an example for a conversion tool.
// By attaching different devices/LUNs multiple printers (i.e. different print commands) are possible.
// Note that the print command is not executed by root but with the permissions of the lp user.
//
// With STOP PRINT printing can be cancelled before SYNCHRONIZE BUFFER was sent.
//
// SEND DIAGNOSTIC currently returns no data.
//

#include <sys/stat.h>
#include "rascsi_exceptions.h"
#include "../rasutil.h"
#include "dispatcher.h"
#include "scsi_printer.h"

using namespace std;
using namespace scsi_defs;
using namespace ras_util;

SCSIPrinter::SCSIPrinter() : PrimaryDevice("SCLP")
{
	dispatcher.Add(scsi_command::eCmdTestUnitReady, "TestUnitReady", &SCSIPrinter::TestUnitReady);
	dispatcher.Add(scsi_command::eCmdReserve6, "ReserveUnit", &SCSIPrinter::ReserveUnit);
	dispatcher.Add(scsi_command::eCmdRelease6, "ReleaseUnit", &SCSIPrinter::ReleaseUnit);
	dispatcher.Add(scsi_command::eCmdWrite6, "Print", &SCSIPrinter::Print);
	dispatcher.Add(scsi_command::eCmdSynchronizeBuffer, "SynchronizeBuffer", &SCSIPrinter::SynchronizeBuffer);
	dispatcher.Add(scsi_command::eCmdSendDiag, "SendDiagnostic", &SCSIPrinter::SendDiagnostic);
	dispatcher.Add(scsi_command::eCmdStartStop, "StopPrint", &SCSIPrinter::StopPrint);
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

	if (!GetAsInt(GetParam("timeout"), timeout) || timeout <= 0) {
		LOGERROR("Reservation timeout value must be > 0")
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
	CheckReservation();

	EnterStatusPhase();
}

vector<byte> SCSIPrinter::InquiryInternal() const
{
	return HandleInquiry(device_type::PRINTER, scsi_level::SCSI_2, false);
}

void SCSIPrinter::ReserveUnit()
{
	// The printer is released after a configurable time in order to prevent deadlocks caused by broken clients
	if (reservation_time + timeout < time(nullptr)) {
		DiscardReservation();
	}

	CheckReservation();

	reserving_initiator = controller->GetInitiatorId();

	if (reserving_initiator != -1) {
		LOGTRACE("Reserved device ID %d, LUN %d for initiator ID %d", GetId(), GetLun(), reserving_initiator)
	}
	else {
		LOGTRACE("Reserved device ID %d, LUN %d for unknown initiator", GetId(), GetLun())
	}

	Cleanup();

	EnterStatusPhase();
}

void SCSIPrinter::ReleaseUnit()
{
	CheckReservation();

	if (reserving_initiator != -1) {
		LOGTRACE("Released device ID %d, LUN %d reserved by initiator ID %d", GetId(), GetLun(), reserving_initiator)
	}
	else {
		LOGTRACE("Released device ID %d, LUN %d reserved by unknown initiator", GetId(), GetLun())
	}

	DiscardReservation();

	EnterStatusPhase();
}

void SCSIPrinter::Print()
{
	CheckReservation();

	uint32_t length = ctrl->cmd[2];
	length <<= 8;
	length |= ctrl->cmd[3];
	length <<= 8;
	length |= ctrl->cmd[4];

	LOGTRACE("Receiving %d byte(s) to be printed", length)

	// TODO This device suffers from the statically allocated buffer size,
	// see https://github.com/akuker/RASCSI/issues/669
	if (length > (uint32_t)ctrl->bufsize) {
		LOGERROR("Transfer buffer overflow")

		throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	ctrl->length = length;
	controller->SetByteTransfer(true);

	EnterDataOutPhase();
}

void SCSIPrinter::SynchronizeBuffer()
{
	CheckReservation();

	if (fd == -1) {
		throw scsi_error_exception();
	}

	// Make the file readable for the lp user
	fchmod(fd, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	struct stat st;
	fstat(fd, &st);

	close(fd);
	fd = -1;

	string cmd = GetParam("cmd");
	size_t file_position = cmd.find("%f");
	assert(file_position != string::npos);
	cmd.replace(file_position, 2, filename);
	cmd = "sudo -u lp " + cmd;

	LOGTRACE("%s", string("Printing file with size of " + to_string(st.st_size) +" byte(s)").c_str())

	LOGDEBUG("Executing '%s'", cmd.c_str())

	if (system(cmd.c_str())) {
		LOGERROR("Printing failed, the printing system might not be configured")

		unlink(filename);

		throw scsi_error_exception();
	}

	unlink(filename);

	EnterStatusPhase();
}

void SCSIPrinter::SendDiagnostic()
{
	// Both command implemntations are identical
	TestUnitReady();
}

void SCSIPrinter::StopPrint()
{
	// Both command implemntations are identical
	TestUnitReady();
}

bool SCSIPrinter::WriteByteSequence(BYTE *buf, uint32_t length)
{
	if (fd == -1) {
		strcpy(filename, TMP_FILE_PATTERN);
		fd = mkstemp(filename);
		if (fd == -1) {
			LOGERROR("Can't create printer output file '%s': %s", filename, strerror(errno))
			return false;
		}

		LOGTRACE("Created printer output file '%s'", filename)
	}

	LOGTRACE("Appending %d byte(s) to printer output file '%s'", length, filename)

	auto num_written = (uint32_t)write(fd, buf, length);

	return num_written == length;
}

void SCSIPrinter::CheckReservation()
{
	if (reserving_initiator == NOT_RESERVED || reserving_initiator == controller->GetInitiatorId()) {
		reservation_time = time(nullptr);
		return;
	}

	if (controller->GetInitiatorId() != -1) {
		LOGTRACE("Initiator ID %d tries to access reserved device ID %d, LUN %d", controller->GetInitiatorId(), GetId(), GetLun())
	}
	else {
		LOGTRACE("Unknown initiator tries to access reserved device ID %d, LUN %d", GetId(), GetLun())
	}

	throw scsi_error_exception(sense_key::ABORTED_COMMAND, asc::NO_ADDITIONAL_SENSE_INFORMATION,
			status::RESERVATION_CONFLICT);
}

void SCSIPrinter::DiscardReservation()
{
	Cleanup();

	reserving_initiator = NOT_RESERVED;
}

void SCSIPrinter::Cleanup()
{
	if (fd != -1) {
		close(fd);
		fd = -1;

		unlink(filename);
	}
}

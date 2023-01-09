//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
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
#include "controllers/scsidev_ctrl.h"
#include "../rasutil.h"
#include "scsi_printer.h"
#include <string>

#define NOT_RESERVED -2

using namespace std;
using namespace scsi_defs;
using namespace ras_util;

SCSIPrinter::SCSIPrinter() : PrimaryDevice("SCLP"), ScsiPrinterCommands()
{
	fd = -1;
	reserving_initiator = NOT_RESERVED;
	reservation_time = 0;
	timeout = 0;

	dispatcher.AddCommand(eCmdTestUnitReady, "TestUnitReady", &SCSIPrinter::TestUnitReady);
	dispatcher.AddCommand(eCmdReserve6, "ReserveUnit", &SCSIPrinter::ReserveUnit);
	dispatcher.AddCommand(eCmdRelease6, "ReleaseUnit", &SCSIPrinter::ReleaseUnit);
	dispatcher.AddCommand(eCmdWrite6, "Print", &SCSIPrinter::Print);
	dispatcher.AddCommand(eCmdSynchronizeBuffer, "SynchronizeBuffer", &SCSIPrinter::SynchronizeBuffer);
	dispatcher.AddCommand(eCmdSendDiag, "SendDiagnostic", &SCSIPrinter::SendDiagnostic);
	dispatcher.AddCommand(eCmdStartStop, "StopPrint", &SCSIPrinter::StopPrint);
}

SCSIPrinter::~SCSIPrinter()
{
	Cleanup();
}

bool SCSIPrinter::Init(const map<string, string>& params)
{
	SetParams(params);

	if (GetParam("cmd").find("%f") == string::npos) {
		LOGERROR("Missing filename specifier %s", "%f");
		return false;
	}

	if (!GetAsInt(GetParam("timeout"), timeout) || timeout <= 0) {
		LOGERROR("Reservation timeout value must be > 0");
		return false;
	}

	return true;
}

bool SCSIPrinter::Dispatch(SCSIDEV *controller)
{
	// The superclass class handles the less specific commands
	return dispatcher.Dispatch(this, controller) ? true : super::Dispatch(controller);
}

void SCSIPrinter::TestUnitReady(SCSIDEV *controller)
{
	if (!CheckReservation(controller)) {
		return;
	}

	controller->Status();
}

int SCSIPrinter::Inquiry(const DWORD *cdb, BYTE *buf)
{
	// Printer device, SCSI-2, not removable
	return PrimaryDevice::Inquiry(2, 2, false, cdb, buf);
}

void SCSIPrinter::ReserveUnit(SCSIDEV *controller)
{
	// The printer is released after a configurable time in order to prevent deadlocks caused by broken clients
	if (reservation_time + timeout < time(0)) {
		DiscardReservation();
	}

	if (!CheckReservation(controller)) {
		return;
	}

	reserving_initiator = controller->GetInitiatorId();

	if (reserving_initiator != -1) {
		LOGTRACE("Reserved device ID %d, LUN %d for initiator ID %d", GetId(), GetLun(), reserving_initiator);
	}
	else {
		LOGTRACE("Reserved device ID %d, LUN %d for unknown initiator", GetId(), GetLun());
	}

	Cleanup();

	controller->Status();
}

void SCSIPrinter::ReleaseUnit(SCSIDEV *controller)
{
	if (!CheckReservation(controller)) {
		return;
	}

	if (reserving_initiator != -1) {
		LOGTRACE("Released device ID %d, LUN %d reserved by initiator ID %d", GetId(), GetLun(), reserving_initiator);
	}
	else {
		LOGTRACE("Released device ID %d, LUN %d reserved by unknown initiator", GetId(), GetLun());
	}

	DiscardReservation();

	controller->Status();
}

void SCSIPrinter::Print(SCSIDEV *controller)
{
	if (!CheckReservation(controller)) {
		return;
	}

	uint32_t length = ctrl->cmd[2];
	length <<= 8;
	length |= ctrl->cmd[3];
	length <<= 8;
	length |= ctrl->cmd[4];

	LOGTRACE("Receiving %d bytes to be printed", length);

	// TODO This device suffers from the statically allocated buffer size,
	// see https://github.com/akuker/RASCSI/issues/669
	if (length > (uint32_t)ctrl->bufsize) {
		LOGERROR("Transfer buffer overflow");

		controller->Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::INVALID_FIELD_IN_CDB);
		return;
	}

	ctrl->length = length;
	controller->SetByteTransfer(true);

	controller->DataOut();
}

void SCSIPrinter::SynchronizeBuffer(SCSIDEV *controller)
{
	if (!CheckReservation(controller)) {
		return;
	}

	if (fd == -1) {
		controller->Error();
		return;
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

	LOGTRACE("%s", string("Printing file with size of " + to_string(st.st_size) +" byte(s)").c_str());

	LOGDEBUG("Executing '%s'", cmd.c_str());

	if (system(cmd.c_str())) {
		LOGERROR("Printing failed, the printing system might not be configured");

		controller->Error();
	}
	else {
		controller->Status();
	}

	unlink(filename);
}

void SCSIPrinter::SendDiagnostic(SCSIDEV *controller)
{
	if (!CheckReservation(controller)) {
		return;
	}

	controller->Status();
}

void SCSIPrinter::StopPrint(SCSIDEV *controller)
{
	if (!CheckReservation(controller)) {
		return;
	}

	// Nothing to do, printing has not yet been started

	controller->Status();
}

bool SCSIPrinter::WriteBytes(BYTE *buf, uint32_t length)
{
	if (fd == -1) {
		strcpy(filename, TMP_FILE_PATTERN);
		fd = mkstemp(filename);
		if (fd == -1) {
			LOGERROR("Can't create printer output file: %s", strerror(errno));
			return false;
		}

		LOGTRACE("Created printer output file '%s'", filename);
	}

	LOGTRACE("Appending %d byte(s) to printer output file", length);

	write(fd, buf, length);

	return true;
}

bool SCSIPrinter::CheckReservation(SCSIDEV *controller)
{
	if (reserving_initiator == NOT_RESERVED || reserving_initiator == controller->GetInitiatorId()) {
		reservation_time = time(0);

		return true;
	}

	if (controller->GetInitiatorId() != -1) {
		LOGTRACE("Initiator ID %d tries to access reserved device ID %d, LUN %d", controller->GetInitiatorId(), GetId(), GetLun());
	}
	else {
		LOGTRACE("Unknown initiator tries to access reserved device ID %d, LUN %d", GetId(), GetLun());
	}

	controller->Error(ERROR_CODES::sense_key::ABORTED_COMMAND, ERROR_CODES::asc::NO_ADDITIONAL_SENSE_INFORMATION,
			ERROR_CODES::status::RESERVATION_CONFLICT);

	return false;
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

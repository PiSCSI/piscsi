//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

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

	dispatcher.AddCommand(eCmdReserve6, "ReserveUnit", &SCSIPrinter::ReserveUnit);
	dispatcher.AddCommand(eCmdRelease6, "ReleaseUnit", &SCSIPrinter::ReleaseUnit);
	dispatcher.AddCommand(eCmdWrite6, "Print", &SCSIPrinter::Print);
	dispatcher.AddCommand(eCmdReadCapacity10, "SynchronizeBuffer", &SCSIPrinter::SynchronizeBuffer);
	dispatcher.AddCommand(eCmdSendDiag, "SendDiagnostic", &SCSIPrinter::SendDiagnostic);
}

bool SCSIPrinter::Init(const map<string, string>& params)
{
	// Use default parameters if no parameters were provided
	SetParams(params.empty() ? GetDefaultParams() : params);

	int t;
	GetAsInt(GetParam("timeout"), t);
	if (t > 0) {
		timeout = t;
	}

	return true;
}

bool SCSIPrinter::Dispatch(SCSIDEV *controller)
{
	// The superclass class handles the less specific commands
	return dispatcher.Dispatch(this, controller) ? true : super::Dispatch(controller);
}

void SCSIPrinter::TestUnitReady(SASIDEV *controller)
{
	if (!CheckReservation(controller)) {
		return;
	}

	controller->Status();
}

int SCSIPrinter::Inquiry(const DWORD *cdb, BYTE *buf)
{
	// Printer device, not removable
	return PrimaryDevice::Inquiry(2, false, cdb, buf);
}

void SCSIPrinter::ReserveUnit(SASIDEV *controller)
{
	if (reservation_time + timeout < time(0)) {
		reserving_initiator = NOT_RESERVED;
	}

	if (!CheckReservation(controller)) {
		return;
	}

	reserving_initiator = ctrl->initiator_id;

	LOGTRACE("Reserved device ID %d, LUN %d for initiator ID %d", GetId(), GetLun(), reserving_initiator);

	if (fd != -1) {
		close(fd);
	}
	fd = -1;

	controller->Status();
}

void SCSIPrinter::ReleaseUnit(SASIDEV *controller)
{
	if (!CheckReservation(controller)) {
		return;
	}

	if (fd != -1) {
		close(fd);
	}
	fd = -1;

	LOGTRACE("Released device ID %d, LUN %d reserved by initiator ID %d", GetId(), GetLun(), reserving_initiator);

	reserving_initiator = NOT_RESERVED;

	controller->Status();
}

void SCSIPrinter::Print(SASIDEV *controller)
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
	if (length > (uint32_t)controller->DEFAULT_BUFFER_SIZE) {
		controller->Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::INVALID_FIELD_IN_CDB);
		return;
	}

	ctrl->length = length;
	ctrl->is_byte_transfer = true;

	controller->DataOut();
}

void SCSIPrinter::SynchronizeBuffer(SASIDEV *controller)
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

	string print_cmd = GetParam("printer");
	print_cmd += " ";
	print_cmd += filename;

	LOGTRACE("%s", string("Printing file with " + to_string(st.st_size) +" byte(s)").c_str());

	LOGDEBUG("Executing '%s'", print_cmd.c_str());

	if (system(print_cmd.c_str())) {
		LOGERROR("Printing failed, the printing system might not be configured");

		controller->Error();
	}
	else {
		controller->Status();
	}

	unlink(filename);
}

void SCSIPrinter::SendDiagnostic(SASIDEV *controller)
{
	if (!CheckReservation(controller)) {
		return;
	}

	controller->Status();
}

bool SCSIPrinter::Write(BYTE *buf, uint32_t length)
{
	if (fd == -1) {
		strcpy(filename, TMP_FILE_PATTERN);
		fd = mkstemp(filename);
		if (fd == -1) {
			LOGERROR("Can't create temporary printer output file: %s", strerror(errno));
			return false;
		}

		LOGTRACE("Created printer output file '%s'", filename);
	}

	LOGTRACE("Appending %d byte(s) to printer output file", length);

	write(fd, buf, length);

	return true;
}

bool SCSIPrinter::CheckReservation(SASIDEV *controller)
{
	if (reserving_initiator == NOT_RESERVED || reserving_initiator == ctrl->initiator_id) {
		reservation_time = time(0);

		return true;
	}

	LOGTRACE("Initiator ID %d tries to access device ID %d, LUN %d reserved by initiator ID %d",
			ctrl->initiator_id, GetId(), GetLun(), reserving_initiator);

	controller->Error(ERROR_CODES::sense_key::ABORTED_COMMAND, ERROR_CODES::asc::NO_ADDITIONAL_SENSE_INFORMATION,
			ERROR_CODES::status::RESERVATION_CONFLICT);

	return false;
}

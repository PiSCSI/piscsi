//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "controllers/scsidev_ctrl.h"
#include "scsi_printer.h"

using namespace scsi_defs;

SCSIPrinter::SCSIPrinter() : PrimaryDevice("SCLP"), ScsiPrinterCommands()
{
	lp_file = NULL;

	dispatcher.AddCommand(eCmdReserve6, "ReserveUnit", &SCSIPrinter::ReserveUnit);
	dispatcher.AddCommand(eCmdRelease6, "ReleaseUnit", &SCSIPrinter::ReleaseUnit);
	dispatcher.AddCommand(eCmdWrite6, "Print", &SCSIPrinter::Print);
	dispatcher.AddCommand(eCmdReadCapacity10, "SynchronizeBuffer", &SCSIPrinter::SynchronizeBuffer);
	dispatcher.AddCommand(eCmdSendDiag, "SendDiagnostic", &SCSIPrinter::SendDiagnostic);
}

bool SCSIPrinter::Dispatch(SCSIDEV *controller)
{
	// The superclass class handles the less specific commands
	return dispatcher.Dispatch(this, controller) ? true : super::Dispatch(controller);
}

void SCSIPrinter::TestUnitReady(SASIDEV *controller)
{
	// TODO Not ready when reserved for a different initiator ID

	// Always successful
	controller->Status();
}

int SCSIPrinter::Inquiry(const DWORD *cdb, BYTE *buf)
{
	// Printer device, not removable
	return PrimaryDevice::Inquiry(2, false, cdb, buf);
}

void SCSIPrinter::ReserveUnit(SASIDEV *controller)
{
	// TODO Implement initiator ID handling when everything else is working

	ctrl->length = 0;
	ctrl->is_byte_transfer = false;

	if (lp_file) {
		fclose(lp_file);
		lp_file = NULL;
	}

	controller->Status();
}

void SCSIPrinter::ReleaseUnit(SASIDEV *controller)
{
	// TODO Implement initiator ID handling when everything else is working

	ctrl->length = 0;
	ctrl->is_byte_transfer = false;

	if (lp_file) {
		fclose(lp_file);
		lp_file = NULL;
	}

	controller->Status();
}

void SCSIPrinter::Print(SASIDEV *controller)
{
	uint32_t length = ctrl->cmd[2];
	length <<= 8;
	length |= ctrl->cmd[3];
	length <<= 8;
	length |= ctrl->cmd[4];

	// TODO The printer device suffers from the statically allocated buffer size
	if (length > (uint32_t)controller->DEFAULT_BUFFER_SIZE) {
		controller->Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::INVALID_FIELD_IN_CDB);
		return;
	}

	ctrl->length = length;
	ctrl->is_byte_transfer = true;

	LOGTRACE("Receiving %d bytes to be printed", length);

	controller->DataOut();
}

void SCSIPrinter::SynchronizeBuffer(SASIDEV *controller)
{
	if (!lp_file) {
		// TODO More specific error handling
		controller->Error();
		return;
	}

	LOGDEBUG("Closing printer file");

	fclose(lp_file);
	lp_file = NULL;

	ctrl->length = 0;
	ctrl->is_byte_transfer = false;

	// TODO Hard-coded filename
	// if (system("lp -oraw /tmp/lp.dat")) {
	// unlink("/tmp/lp.dat");
	// TODO Error handling
	// controller->Error();
	// return;
	// }

	unlink("/tmp/lp.dat");

	controller->Status();
}

void SCSIPrinter::SendDiagnostic(SASIDEV *controller)
{
	// TODO Implement when everything else is working
	controller->Status();
}

bool SCSIPrinter::Write(BYTE *buf, uint32_t length)
{
	if (!lp_file) {
		LOGDEBUG("Opening printer file");

		lp_file = fopen("/tmp/lp.dat", "wb");
	}

	LOGDEBUG("Adding %d byte(s) to printer file", length);

	fwrite(buf, 1, length, lp_file);

	return true;
}

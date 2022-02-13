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

SCSIPrinter::SCSIPrinter() : PrimaryDevice("SCLP")
{
	current_file = NULL;

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
	// TODO Implement when everything else is working
	controller->Status();
}

void SCSIPrinter::ReleaseUnit(SASIDEV *controller)
{
	// TODO Implement when everything else is working
	controller->Status();
}

void SCSIPrinter::Print(SASIDEV *controller)
{
	uint32_t length = ctrl->cmd[2];
	length <<= 8;
	length |= ctrl->cmd[3];
	length <<= 8;
	length |= ctrl->cmd[4];
	if (!length) {
		controller->Status();
		return;
	}

	((SCSIDEV *)controller)->DataOutScsi();

	// RaSCSI currently only supports transfers with multiples of 512 bytes. This has to be changed
	// when everything else is working.

	// TODO Implement

	controller->Status();
}

void SCSIPrinter::SynchronizeBuffer(SASIDEV *controller)
{
	// TODO Implement
	controller->Status();
}

void SCSIPrinter::SendDiagnostic(SASIDEV *controller)
{
	// TODO Implement when everything else is working
	controller->Status();
}

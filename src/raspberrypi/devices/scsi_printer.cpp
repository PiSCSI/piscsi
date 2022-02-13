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
	dispatcher.AddCommand(eCmdWrite6, "Print", &SCSIPrinter::Print);
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

void SCSIPrinter::Print(SASIDEV *controller)
{
	controller->Status();
}

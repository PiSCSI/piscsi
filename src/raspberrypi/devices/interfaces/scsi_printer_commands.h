//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
// Interface for SCSI printer commands (see SCSI-2 specification)
//
//---------------------------------------------------------------------------

#pragma once

#include "scsi_primary_commands.h"

class SASIDEV;

class ScsiPrinterCommands : virtual public ScsiPrimaryCommands
{
public:

	ScsiPrinterCommands() {}
	virtual ~ScsiPrinterCommands() {}

	// Mandatory commands
	virtual void Print(SASIDEV *) = 0;
	virtual void ReleaseUnit(SASIDEV *) = 0;
	virtual void ReserveUnit(SASIDEV *) = 0;
	virtual void SendDiagnostic(SASIDEV *) = 0;
};

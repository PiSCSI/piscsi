//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
// Interface for SCSI printer commands (see SCSI-2 specification)
//
//---------------------------------------------------------------------------

#pragma once

#include "scsi_primary_commands.h"

class ScsiPrinterCommands : virtual public ScsiPrimaryCommands
{
public:

	ScsiPrinterCommands() = default;
	~ScsiPrinterCommands() override = default;

	// Mandatory commands
	virtual void Print() = 0;
	virtual void ReleaseUnit() = 0;
	virtual void ReserveUnit() = 0;
	virtual void SendDiagnostic() = 0;
};

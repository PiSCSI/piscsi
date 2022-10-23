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

class ScsiPrinterCommands
{

public:

	ScsiPrinterCommands() = default;
	virtual ~ScsiPrinterCommands() = default;

	// Mandatory commands
	virtual void Print() = 0;
	virtual void ReleaseUnit() = 0;
	virtual void ReserveUnit() = 0;
	virtual void SendDiagnostic() = 0;
};

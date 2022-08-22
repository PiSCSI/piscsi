//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
// Interface for SCSI block commands (see https://www.t10.org/drafts.htm, SBC-5)
//
//---------------------------------------------------------------------------

#pragma once

#include "scsi_primary_commands.h"

class Controller;

class ScsiBlockCommands : virtual public ScsiPrimaryCommands
{
public:

	ScsiBlockCommands() {}
	virtual ~ScsiBlockCommands() {}

	// Mandatory commands
	virtual void FormatUnit(Controller *) = 0;
	virtual void ReadCapacity10(Controller *) = 0;
	virtual void ReadCapacity16(Controller *) = 0;
	virtual void Read10(Controller *) = 0;
	virtual void Read16(Controller *) = 0;
	virtual void Write10(Controller *) = 0;
	virtual void Write16(Controller *) = 0;
};

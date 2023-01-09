//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
// Interface for SCSI block commands (see https://www.t10.org/drafts.htm, SBC-5)
//
//---------------------------------------------------------------------------

#pragma once

#include "scsi_primary_commands.h"

class SASIDEV;

class ScsiBlockCommands : virtual public ScsiPrimaryCommands
{
public:

	ScsiBlockCommands() {}
	virtual ~ScsiBlockCommands() {}

	// Mandatory commands
	virtual void FormatUnit(SASIDEV *) = 0;
	virtual void ReadCapacity10(SASIDEV *) = 0;
	virtual void ReadCapacity16(SASIDEV *) = 0;
	virtual void Read10(SASIDEV *) = 0;
	virtual void Read16(SASIDEV *) = 0;
	virtual void Write10(SASIDEV *) = 0;
	virtual void Write16(SASIDEV *) = 0;
};

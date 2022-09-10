//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
// Interface for SCSI block commands (see https://www.t10.org/drafts.htm, SBC-5)
//
//---------------------------------------------------------------------------

#pragma once

class ScsiBlockCommands
{

public:

	ScsiBlockCommands() = default;
	virtual ~ScsiBlockCommands() = default;

	// Mandatory commands
	virtual void FormatUnit() = 0;
	virtual void ReadCapacity10() = 0;
	virtual void ReadCapacity16() = 0;
	virtual void Read10() = 0;
	virtual void Read16() = 0;
	virtual void Write10() = 0;
	virtual void Write16() = 0;
};

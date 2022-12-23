//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
// Interface for SCSI Multi-Media commands (see https://www.t10.org/drafts.htm, MMC-6)
//
//---------------------------------------------------------------------------

#pragma once

class ScsiMmcCommands
{

public:

	ScsiMmcCommands() = default;
	virtual ~ScsiMmcCommands() = default;

	virtual void ReadToc() = 0;
};

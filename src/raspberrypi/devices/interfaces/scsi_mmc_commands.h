//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
// Interface for SCSI Multi-Media commands (see https://www.t10.org/drafts.htm, MMC-6)
//
//---------------------------------------------------------------------------

#pragma once

#include "scsi_primary_commands.h"

class SASIDEV;

class ScsiMmcCommands : virtual public ScsiPrimaryCommands
{
public:

	ScsiMmcCommands() {}
	virtual ~ScsiMmcCommands() {}

	virtual void ReadToc(SASIDEV *) = 0;
	virtual void GetEventStatusNotification(SASIDEV *) = 0;
};

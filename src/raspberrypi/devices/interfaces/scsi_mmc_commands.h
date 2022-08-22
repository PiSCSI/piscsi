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

class Controller;

class ScsiMmcCommands : virtual public ScsiPrimaryCommands
{
public:

	ScsiMmcCommands() {}
	virtual ~ScsiMmcCommands() {}

	virtual void ReadToc(Controller *) = 0;
	virtual void GetEventStatusNotification(Controller *) = 0;
};

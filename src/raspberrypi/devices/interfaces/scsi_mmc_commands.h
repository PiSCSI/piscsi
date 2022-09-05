//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
// Interface for SCSI Multi-Media commands (see https://www.t10.org/drafts.htm, MMC-6)
//
//---------------------------------------------------------------------------

#pragma once

#include "scsi_primary_commands.h"

class ScsiMmcCommands : virtual public ScsiPrimaryCommands
{
public:

	ScsiMmcCommands() = default;
	~ScsiMmcCommands() override = default;

	virtual void ReadToc() = 0;
	virtual void GetEventStatusNotification() = 0;
};

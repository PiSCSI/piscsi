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

class SASIDEV;

class ScsiMmcCommands
{
public:

	ScsiMmcCommands() {};
	virtual ~ScsiMmcCommands() {};

	virtual void ReadToc(SASIDEV *) = 0;
	virtual void GetEventStatusNotification(SASIDEV *) = 0;
};

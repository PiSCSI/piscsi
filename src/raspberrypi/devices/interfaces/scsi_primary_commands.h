//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
// Interface for SCSI primary commands (see https://www.t10.org/drafts.htm, SPC-6)
//
//---------------------------------------------------------------------------

#pragma once

class SCSIDEV;

class ScsiPrimaryCommands
{
public:

	ScsiPrimaryCommands() {}
	virtual ~ScsiPrimaryCommands() {}

	// Mandatory commands
	virtual void TestUnitReady(SCSIDEV *) = 0;
	virtual void Inquiry(SCSIDEV *) = 0;
	virtual void ReportLuns(SCSIDEV *) = 0;

	// Implemented for all RaSCSI device types
	virtual void RequestSense(SCSIDEV *) = 0;
};

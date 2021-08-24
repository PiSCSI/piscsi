//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
// A PrimaryDevice supports SCSI primary commands (see https://www.t10.org/drafts.htm, SPC-6)
//
//---------------------------------------------------------------------------

#pragma once

class SASIDEV;

class PrimaryDevice
{
public:

	PrimaryDevice() {};
	virtual ~PrimaryDevice() {};

	// Mandatory commands
	virtual void TestUnitReady(SASIDEV *) = 0;
	virtual void Inquiry(SASIDEV *) = 0;
	virtual void ReportLuns(SASIDEV *) = 0;

	// Implemented optional commands
	virtual void RequestSense(SASIDEV *) = 0;
	virtual void ModeSense(SASIDEV *) = 0;
	virtual void ModeSense10(SASIDEV *) = 0;
	virtual void ModeSelect(SASIDEV *) = 0;
	virtual void ModeSelect10(SASIDEV *) = 0;
};

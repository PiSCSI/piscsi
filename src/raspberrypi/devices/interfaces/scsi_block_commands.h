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

class SASIDEV;

class ScsiBlockCommands
{
public:

	ScsiBlockCommands() {};
	virtual ~ScsiBlockCommands() {};

	// Mandatory commands
	virtual void TestUnitReady(SASIDEV *) = 0;
	virtual void Inquiry(SASIDEV *) = 0;
	virtual void ReportLuns(SASIDEV *) = 0;
	virtual void FormatUnit(SASIDEV *) = 0;
	virtual void ReadCapacity10(SASIDEV *) = 0;
	virtual void ReadCapacity16(SASIDEV *) = 0;
	virtual void Read10(SASIDEV *) = 0;
	virtual void Read16(SASIDEV *) = 0;
	virtual void Write10(SASIDEV *) = 0;
	virtual void Write16(SASIDEV *) = 0;
	virtual void RequestSense(SASIDEV *) = 0;

	// Implemented optional commands
	virtual void ReadLong10(SASIDEV *) = 0;
	virtual void WriteLong10(SASIDEV *) = 0;
	virtual void Verify10(SASIDEV *) = 0;
	virtual void ReadLong16(SASIDEV *) = 0;
	virtual void WriteLong16(SASIDEV *) = 0;
	virtual void Verify16(SASIDEV *) = 0;
	virtual void ModeSense6(SASIDEV *) = 0;
	virtual void ModeSense10(SASIDEV *) = 0;
	virtual void ModeSelect6(SASIDEV *) = 0;
	virtual void ModeSelect10(SASIDEV *) = 0;
	virtual void ReassignBlocks(SASIDEV *) = 0;
	virtual void SendDiagnostic(SASIDEV *) = 0;
	virtual void StartStopUnit(SASIDEV *) = 0;
	virtual void SynchronizeCache10(SASIDEV *) = 0;
	virtual void SynchronizeCache16(SASIDEV *) = 0;
};

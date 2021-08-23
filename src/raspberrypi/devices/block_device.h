//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
// A BlockDevice supports SCSI block commands (see https://www.t10.org/drafts.htm, SBC-5)
//
//---------------------------------------------------------------------------

#pragma once

#include "primary_device.h"

class SASIDEV;

class BlockDevice : public PrimaryDevice
{
public:

	BlockDevice(const string& id) : PrimaryDevice(id) {};
	virtual ~BlockDevice() {};

	// Mandatory commands
	virtual void TestUnitReady(SASIDEV *) override = 0;
	virtual void Inquiry(SASIDEV *) override = 0;
	virtual void ReportLuns(SASIDEV *) override = 0;
	virtual void FormatUnit(SASIDEV *) = 0;
	virtual void ReadCapacity10(SASIDEV *) = 0;
	virtual void ReadCapacity16(SASIDEV *) = 0;
	virtual void Read10(SASIDEV *) = 0;
	virtual void Read16(SASIDEV *) = 0;
	virtual void Write10(SASIDEV *) = 0;
	virtual void Write16(SASIDEV *) = 0;
	virtual void RequestSense(SASIDEV *) override = 0;

	// Implemented optional commands
	virtual void Verify10(SASIDEV *) = 0;
	// TODO Implement
	// virtual void Verify16(SASIDEV *) = 0;
	virtual void ModeSense(SASIDEV *) override = 0;
	virtual void ModeSense10(SASIDEV *) override = 0;
	virtual void ModeSelect(SASIDEV *) override = 0;
	virtual void ModeSelect10(SASIDEV *) override = 0;
	virtual void ReassignBlocks(SASIDEV *) = 0;
	virtual void SendDiagnostic(SASIDEV *) = 0;
	virtual void StartStopUnit(SASIDEV *) = 0;
	virtual void SynchronizeCache10(SASIDEV *) = 0;
	virtual void SynchronizeCache16(SASIDEV *) = 0;
};

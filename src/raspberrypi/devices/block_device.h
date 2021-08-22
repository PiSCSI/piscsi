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
	virtual bool TestUnitReady(const DWORD *cdb) override = 0;
	virtual int Inquiry(const DWORD *cdb, BYTE *buf) override = 0;
	virtual void ReportLuns(SASIDEV *) override = 0;
	virtual bool Format(const DWORD *cdb) = 0;
	virtual void ReadCapacity10(SASIDEV *) = 0;
	virtual void ReadCapacity16(SASIDEV *) = 0;
	virtual void Read10(SASIDEV *) = 0;
	virtual void Read16(SASIDEV *) = 0;
	virtual void Write10(SASIDEV *) = 0;
	virtual void Write16(SASIDEV *) = 0;

	// Implemented optional commands
	// TODO uncomment
	//virtual void Verify10(SASIDEV *) = 0;
	//virtual void Verify16(SASIDEV *) = 0;
	virtual int RequestSense(const DWORD *cdb, BYTE *buf) override = 0;
	virtual int ModeSense(const DWORD *cdb, BYTE *buf) override = 0;
	virtual int ModeSense10(const DWORD *cdb, BYTE *buf) override = 0;
	virtual bool ModeSelect(const DWORD *cdb, const BYTE *buf, int length) override = 0;

	// TODO Add the other optional commands currently implemented
};

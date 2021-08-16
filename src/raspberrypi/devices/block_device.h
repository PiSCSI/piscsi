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

class BlockDevice : public PrimaryDevice
{
public:

	BlockDevice(const string& id) : PrimaryDevice(id) {};
	virtual ~BlockDevice() {};

	// Mandatory commands
	// TODO Add the mandatory command REPORT LUNS
	virtual bool TestUnitReady(const DWORD *cdb) = 0;
	virtual int Inquiry(const DWORD *cdb, BYTE *buf) = 0;
	virtual int RequestSense(const DWORD *cdb, BYTE *buf) = 0;
	virtual bool FormatUnit(const DWORD *cdb) = 0;
	// READ(6), READ(10)
	virtual int Read(const DWORD *cdb, BYTE *buf, DWORD block) = 0;
	// WRITE(6), WRITE(10)
	virtual bool Write(const DWORD *cdb, const BYTE *buf, DWORD block) = 0;
	virtual int ReadCapacity10(const DWORD *cdb, BYTE *buf) = 0;
	virtual int ReadCapacity16(const DWORD *cdb, BYTE *buf) = 0;
	// TODO Uncomment as soon as there is a clean separation between controllers and devices
	//virtual int Read16(const DWORD *cdb, BYTE *buf, DWORD block) = 0;
	//virtual int Write16(const DWORD *cdb, BYTE *buf, DWORD block) = 0;
	//virtual int Verify16(const DWORD *cdb, BYTE *buf, DWORD block) = 0;

	// Optional commands
	// TODO Add the optional commands currently implemented
};

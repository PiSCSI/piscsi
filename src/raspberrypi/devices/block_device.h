//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
// A BlockDevice supports SCSI block commands (see https://www.t10.org/drafts.htm)
//
//---------------------------------------------------------------------------

#pragma once

#include "primary_device.h"

// TODO Add more block commands
class BlockDevice : public PrimaryDevice
{
public:

	BlockDevice(const string& id) : PrimaryDevice(id) {};
	virtual ~BlockDevice() {};

	// READ(6), READ(10)
	virtual int Read(const DWORD *cdb, BYTE *buf, DWORD block) = 0;
	// WRITE(6), WRITE(10)
	virtual bool Write(const DWORD *cdb, const BYTE *buf, DWORD block) = 0;
};

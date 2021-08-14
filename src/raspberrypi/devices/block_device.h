//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "os.h"
#include "primary_device.h"

// TODO Add more block device commands
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

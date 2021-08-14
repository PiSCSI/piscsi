//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "device.h"

// A device supporting SCSI primary commands (see https://www.t10.org/drafts.htm)
class PrimaryDevice : public Device
{
public:

	PrimaryDevice(const string& id) : Device(id) {};
	virtual ~PrimaryDevice() {};

	virtual bool TestUnitReady(const DWORD *cdb) = 0;
	virtual int Inquiry(const DWORD *cdb, BYTE *buf) = 0;
	virtual int RequestSense(const DWORD *cdb, BYTE *buf) = 0;
};

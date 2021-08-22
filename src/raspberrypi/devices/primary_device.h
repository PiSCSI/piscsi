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

#include "controllers/sasidev_ctrl.h"
#include "device.h"

class PrimaryDevice : public Device
{
public:

	PrimaryDevice(const string& id) : Device(id) {};
	virtual ~PrimaryDevice() {};

	// Mandatory commands
	virtual bool TestUnitReady(const DWORD *cdb) = 0;
	virtual int Inquiry(const DWORD *cdb, BYTE *buf) = 0;
	virtual void ReportLuns(SASIDEV *, SASIDEV::ctrl_t *) = 0;

	// Implemented optional commands
	virtual int RequestSense(const DWORD *cdb, BYTE *buf) = 0;
	virtual int ModeSense(const DWORD *cdb, BYTE *buf) = 0;
	virtual int ModeSense10(const DWORD *cdb, BYTE *buf) = 0;
	virtual bool ModeSelect(const DWORD *cdb, const BYTE *buf, int length) = 0;
};

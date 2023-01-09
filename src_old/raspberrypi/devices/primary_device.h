//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
// A device implementing mandatory SCSI primary commands, to be used for subclassing
//
//---------------------------------------------------------------------------

#pragma once

#include "controllers/scsidev_ctrl.h"
#include "interfaces/scsi_primary_commands.h"
#include "device.h"
#include "dispatcher.h"
#include <string>

using namespace std;

class PrimaryDevice: public Device, virtual public ScsiPrimaryCommands
{
public:

	PrimaryDevice(const string&);
	virtual ~PrimaryDevice() {}

	virtual bool Dispatch(SCSIDEV *);

	void TestUnitReady(SASIDEV *);
	void RequestSense(SASIDEV *);

	void SetCtrl(SASIDEV::ctrl_t *ctrl) { this->ctrl = ctrl; }

	bool CheckReady();
	virtual int Inquiry(const DWORD *, BYTE *) = 0;
	virtual int RequestSense(const DWORD *, BYTE *);
	virtual bool WriteBytes(BYTE *, uint32_t);
	virtual int GetSendDelay() { return BUS::SEND_NO_DELAY; }

protected:

	int Inquiry(int, int, bool, const DWORD *, BYTE *);

	SASIDEV::ctrl_t *ctrl;

private:

	Dispatcher<PrimaryDevice, SASIDEV> dispatcher;

	void Inquiry(SASIDEV *);
	void ReportLuns(SASIDEV *);
};

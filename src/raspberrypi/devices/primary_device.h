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

#include "controllers/sasidev_ctrl.h"
#include "interfaces/scsi_primary_commands.h"
#include "device.h"
#include "dispatcher.h"
#include <string>
#include <set>
#include <map>

using namespace std;

class PrimaryDevice: public Device, virtual public ScsiPrimaryCommands
{
public:

	PrimaryDevice(const string&);
	virtual ~PrimaryDevice() {}

	virtual bool Dispatch(SCSIDEV *) override;

	void TestUnitReady(SASIDEV *);
	void RequestSense(SASIDEV *);

	void SetCtrl(SASIDEV::ctrl_t *ctrl) { this->ctrl = ctrl; }

	bool CheckReady();
	virtual int Inquiry(const DWORD *, BYTE *) = 0;
	virtual int RequestSense(const DWORD *, BYTE *);

protected:

	int Inquiry(int, bool, const DWORD *, BYTE *);

	SASIDEV::ctrl_t *ctrl;

private:

	Dispatcher<PrimaryDevice> dispatcher;

	void Inquiry(SASIDEV *);
	void ReportLuns(SASIDEV *);
};

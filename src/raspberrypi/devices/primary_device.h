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

	void TestUnitReady(SCSIDEV *);
	void RequestSense(SCSIDEV *);
	virtual void Inquiry(SCSIDEV *);

	void SetCtrl(SCSIDEV::ctrl_t *ctrl) { this->ctrl = ctrl; }

	bool CheckReady();
	virtual vector<BYTE> Inquiry() const = 0;
	virtual vector<BYTE> RequestSense(int);
	virtual bool WriteBytes(BYTE *, uint32_t);
	virtual int GetSendDelay() const { return BUS::SEND_NO_DELAY; }

protected:

	vector<BYTE> Inquiry(scsi_defs::device_type, scsi_level, bool) const;

	SCSIDEV::ctrl_t *ctrl;

private:

	Dispatcher<PrimaryDevice, SCSIDEV> dispatcher;

	void ReportLuns(SCSIDEV *);
};

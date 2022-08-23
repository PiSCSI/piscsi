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

#include "interfaces/scsi_primary_commands.h"
#include "controller.h"
#include "device.h"
#include "dispatcher.h"
#include <string>

using namespace std;

// TODO The controller should become a member of this class
class PrimaryDevice: public Device, virtual public ScsiPrimaryCommands
{
public:

	PrimaryDevice(const string&);
	virtual ~PrimaryDevice() {}

	virtual bool Dispatch(Controller *);

	void TestUnitReady(Controller *);
	void RequestSense(Controller *);
	virtual void Inquiry(Controller *);

	void SetController(Controller *controller) { this->controller = controller;  }
	void SetCtrl(Controller::ctrl_t *ctrl) { this->ctrl = ctrl; }

	void CheckReady();
	vector<BYTE> RequestSense();
	virtual vector<BYTE> Inquiry() const = 0;
	virtual bool WriteBytes(BYTE *, uint32_t);
	virtual int GetSendDelay() const { return BUS::SEND_NO_DELAY; }

protected:

	vector<BYTE> Inquiry(scsi_defs::device_type, scsi_level, bool) const;

	Controller *controller;
	Controller::ctrl_t *ctrl;

private:

	Dispatcher<PrimaryDevice> dispatcher;

	void ReportLuns(Controller *);
};

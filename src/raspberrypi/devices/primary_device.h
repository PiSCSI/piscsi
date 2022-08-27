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
#include "device.h"
#include "scsi_controller.h"
#include "dispatcher.h"
#include <string>

using namespace std;

class PrimaryDevice: public Device, virtual public ScsiPrimaryCommands
{
public:

	PrimaryDevice(const string&);
	virtual ~PrimaryDevice() {}

	virtual bool Dispatch();

	void SetController(Controller *);
	virtual bool WriteBytes(BYTE *, uint32_t);
	virtual int GetSendDelay() const { return BUS::SEND_NO_DELAY; }

protected:

	vector<BYTE> HandleInquiry(scsi_defs::device_type, scsi_level, bool) const;
	virtual vector<BYTE> InquiryInternal() const = 0;
	void CheckReady();

	void EnterStatusPhase() { phase_handler->Status(); }
	void EnterDataInPhase() { phase_handler->DataIn(); }
	void EnterDataOutPhase() { phase_handler->DataOut(); }

	Controller *controller;
	Controller::ctrl_t *ctrl;

private:

	void TestUnitReady();
	void RequestSense();
	void ReportLuns();
	void Inquiry();
	vector<BYTE> HandleRequestSense();

	Dispatcher<PrimaryDevice> dispatcher;

	// Preparation for decoupling the bus phase handling
	PhaseHandler *phase_handler;
};

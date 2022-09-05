//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
// A device implementing mandatory SCSI primary commands, to be used for subclassing
//
//---------------------------------------------------------------------------

#pragma once

#include "interfaces/scsi_primary_commands.h"
#include "controllers/scsi_controller.h"
#include "device.h"
#include "dispatcher.h"
#include <string>

using namespace std;

class PrimaryDevice: public Device, virtual public ScsiPrimaryCommands
{
public:

	PrimaryDevice(const string&);
	virtual ~PrimaryDevice() = default;

	virtual bool Dispatch();

	void SetController(AbstractController *);
	virtual bool WriteByteSequence(BYTE *, uint32_t);
	virtual int GetSendDelay() const { return BUS::SEND_NO_DELAY; }

protected:

	vector<BYTE> HandleInquiry(scsi_defs::device_type, scsi_level, bool) const;
	virtual vector<BYTE> InquiryInternal() const = 0;
	void CheckReady();

	void EnterStatusPhase() { controller->Status(); }
	void EnterDataInPhase() { controller->DataIn(); }
	void EnterDataOutPhase() { controller->DataOut(); }

	AbstractController *controller = nullptr;
	AbstractController::ctrl_t *ctrl = nullptr;

private:

	void TestUnitReady();
	void RequestSense();
	void ReportLuns();
	void Inquiry();

	vector<BYTE> HandleRequestSense();

	Dispatcher<PrimaryDevice> dispatcher;
};

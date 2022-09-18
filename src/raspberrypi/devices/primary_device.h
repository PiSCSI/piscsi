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

class PrimaryDevice: public ScsiPrimaryCommands, public Device
{

public:

	explicit PrimaryDevice(const string&);
	~PrimaryDevice() override = default;
	PrimaryDevice(PrimaryDevice&) = delete;
	PrimaryDevice& operator=(const PrimaryDevice&) = delete;

	bool Dispatch() override;

	void SetController(AbstractController *);
	virtual bool WriteByteSequence(BYTE *, uint32_t);
	virtual int GetSendDelay() const { return BUS::SEND_NO_DELAY; }

protected:

	vector<byte> HandleInquiry(scsi_defs::device_type, scsi_level, bool) const;
	virtual vector<byte> InquiryInternal() const = 0;
	void CheckReady();

	void EnterStatusPhase() { controller->Status(); }
	void EnterDataInPhase() { controller->DataIn(); }
	void EnterDataOutPhase() { controller->DataOut(); }

	AbstractController *controller = nullptr;
	AbstractController::ctrl_t *ctrl = nullptr;

private:

	void TestUnitReady() override;
	void RequestSense() override;
	void ReportLuns() override;
	void Inquiry() override;

	vector<BYTE> HandleRequestSense() const;

	Dispatcher<PrimaryDevice> dispatcher;
};

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
#include "controllers/abstract_controller.h"
#include "device.h"
#include "dispatcher.h"
#include <string>

class PrimaryDevice: public ScsiPrimaryCommands, public Device
{
public:

	PrimaryDevice(const string&, int);
	~PrimaryDevice() override = default;

	virtual bool Dispatch(scsi_command);

	int GetId() const override;

	void SetController(AbstractController *);
	virtual bool WriteByteSequence(vector<BYTE>&, uint32_t);
	virtual int GetSendDelay() const { return BUS::SEND_NO_DELAY; }

	// Override for device specific initializations, to be called after all device properties have been set
	virtual bool Init(const unordered_map<string, string>&) { return true; };

	virtual void FlushCache() {
		// Devices with a cache have to implement this method
	}

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

	vector<byte> HandleRequestSense() const;

	Dispatcher<PrimaryDevice> dispatcher;
};

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

class PrimaryDevice: private ScsiPrimaryCommands, public Device
{
public:

	PrimaryDevice(PbDeviceType, int);
	~PrimaryDevice() override = default;

	virtual bool Dispatch(scsi_command);

	int GetId() const override;

	void SetController(AbstractController *);
	virtual bool WriteByteSequence(vector<BYTE>&, uint32_t);

	int GetSendDelay() const { return send_delay; }

	bool CheckReservation(int, scsi_command, bool) const;
	void DiscardReservation();

	// Override for device specific initializations
	virtual bool Init(const unordered_map<string, string>&) { return false; };
	void Reset() override;

	virtual void FlushCache() {
		// Devices with a cache have to implement this method
	}

protected:

	vector<byte> HandleInquiry(scsi_defs::device_type, scsi_level, bool) const;
	virtual vector<byte> InquiryInternal() const = 0;
	void CheckReady();

	void SetSendDelay(int s) { send_delay = s; }

	virtual void SendDiagnostic();
	virtual void ReserveUnit();
	virtual void ReleaseUnit();

	void EnterStatusPhase() { controller->Status(); }
	void EnterDataInPhase() { controller->DataIn(); }
	void EnterDataOutPhase() { controller->DataOut(); }

	AbstractController *controller = nullptr;

private:

	static const int NOT_RESERVED = -2;

	void TestUnitReady() override;
	void RequestSense() override;
	void ReportLuns() override;
	void Inquiry() override;

	vector<byte> HandleRequestSense() const;

	Dispatcher<PrimaryDevice> dispatcher;

	int send_delay = BUS::SEND_NO_DELAY;

	int reserving_initiator = NOT_RESERVED;
};

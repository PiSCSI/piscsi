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

#include "shared/scsi.h"
#include "interfaces/scsi_primary_commands.h"
#include "controllers/abstract_controller.h"
#include "device.h"
#include <string>
#include <unordered_map>
#include <functional>

using namespace std;
using namespace scsi_defs;

class PrimaryDevice: private ScsiPrimaryCommands, public Device
{
	using operation = function<void()>;

public:

	PrimaryDevice(PbDeviceType type, int lun) : Device(type, lun) {}
	~PrimaryDevice() override = default;

	virtual bool Init(const unordered_map<string, string>&);

	virtual void Dispatch(scsi_command);

	int GetId() const override;

	void SetController(shared_ptr<AbstractController>);

	virtual bool WriteByteSequence(vector<uint8_t>&, uint32_t);

	int GetSendDelay() const { return send_delay; }

	bool CheckReservation(int, scsi_command, bool) const;
	void DiscardReservation();

	void Reset() override;

	virtual void FlushCache() {
		// Devices with a cache have to implement this method
	}

protected:

	void AddCommand(scsi_command, const operation&);

	vector<uint8_t> HandleInquiry(scsi_defs::device_type, scsi_level, bool) const;
	virtual vector<uint8_t> InquiryInternal() const = 0;
	void CheckReady();

	void SetSendDelay(int s) { send_delay = s; }

	void SendDiagnostic() override;
	void ReserveUnit() override;
	void ReleaseUnit() override;

	void EnterStatusPhase() const { controller.lock()->Status(); }
	void EnterDataInPhase() const { controller.lock()->DataIn(); }
	void EnterDataOutPhase() const { controller.lock()->DataOut(); }

	inline shared_ptr<AbstractController> GetController() const { return controller.lock(); }

private:

	static const int NOT_RESERVED = -2;

	void TestUnitReady() override;
	void RequestSense() override;
	void ReportLuns() override;
	void Inquiry() override;

	vector<byte> HandleRequestSense() const;

	weak_ptr<AbstractController> controller;

	unordered_map<scsi_command, operation> commands;

	int send_delay = BUS::SEND_NO_DELAY;

	int reserving_initiator = NOT_RESERVED;
};

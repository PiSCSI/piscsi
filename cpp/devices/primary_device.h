//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
// A device implementing mandatory SCSI primary commands, to be used for subclassing
//
//---------------------------------------------------------------------------

#pragma once

#include "shared/scsi.h"
#include "interfaces/scsi_primary_commands.h"
#include "controllers/abstract_controller.h"
#include "device.h"
#include "device_logger.h"
#include <string>
#include <unordered_map>
#include <span>
#include <functional>

using namespace std;
using namespace scsi_defs;

class PrimaryDevice: private ScsiPrimaryCommands, public Device
{
	friend class AbstractController;

	using operation = function<void()>;

public:

	PrimaryDevice(PbDeviceType type, int lun) : Device(type, lun) {}
	~PrimaryDevice() override = default;

	virtual bool Init(const param_map&);
	virtual void CleanUp() {
		// Override if cleanup work is required for a derived device
	}

	virtual void Dispatch(scsi_command);

	int GetId() const override;

	virtual bool WriteByteSequence(span<const uint8_t>);

	int GetSendDelay() const { return send_delay; }

	bool CheckReservation(int, scsi_command, bool) const;
	void DiscardReservation();

	void Reset() override;

	virtual void FlushCache() {
		// Devices with a cache have to override this method
	}

	virtual vector<PbStatistics> GetStatistics() const {
		// Devices which provide statistics have to override this method

		return vector<PbStatistics>();
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

	void EnterStatusPhase() const { controller->Status(); }
	void EnterDataInPhase() const { controller->DataIn(); }
	void EnterDataOutPhase() const { controller->DataOut(); }

	auto GetController() const { return controller; }

	void LogTrace(const string& s) const { device_logger.Trace(s); }
	void LogDebug(const string& s) const { device_logger.Debug(s); }
	void LogInfo(const string& s) const { device_logger.Info(s); }
	void LogWarn(const string& s) const { device_logger.Warn(s); }
	void LogError(const string& s) const { device_logger.Error(s); }

private:

	static const int NOT_RESERVED = -2;

	void SetController(AbstractController *);

	void TestUnitReady() override;
	void RequestSense() override;
	void ReportLuns() override;
	void Inquiry() override;

	vector<byte> HandleRequestSense() const;

	// TODO Try to remove this field and use controller->Log*() methods instead
	DeviceLogger device_logger;

	// Owned by the controller manager
	AbstractController *controller = nullptr;

	unordered_map<scsi_command, operation> commands;

	int send_delay = BUS::SEND_NO_DELAY;

	int reserving_initiator = NOT_RESERVED;
};

//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
// Base class for device controllers
//
//---------------------------------------------------------------------------

#pragma once

#include "phase_handler.h"
#include <unordered_map>
#include <vector>
#include <memory>

using namespace std;

class PrimaryDevice;

class AbstractController : public PhaseHandler
{
public:

	// Maximum number of logical units
	static const int LUN_MAX = 32;

	enum class rascsi_shutdown_mode {
		NONE,
		STOP_RASCSI,
		STOP_PI,
		RESTART_PI
	};

	using ctrl_t = struct _ctrl_t {
		// General
		BUS::phase_t phase = BUS::phase_t::busfree;	// Transition phase

		// commands
		vector<int> cmd;				// Command data
		uint32_t status;				// Status data
		int message;					// Message data

		// Transfer
		BYTE *buffer;					// Transfer data buffer
		int bufsize;					// Transfer data buffer size
		uint32_t blocks;				// Number of transfer blocks
		uint64_t next;					// Next record
		uint32_t offset;				// Transfer offset
		uint32_t length;				// Transfer remaining length

		// Logical units of this device controller mapped to their LUN numbers
		unordered_map<int, PrimaryDevice *> luns;
	};

	AbstractController(shared_ptr<BUS> bus, int target_id) : target_id(target_id), bus(bus) {}
	~AbstractController() override = default;
	AbstractController(AbstractController&) = delete;
	AbstractController& operator=(const AbstractController&) = delete;

	virtual BUS::phase_t Process(int) = 0;

	virtual void Error(scsi_defs::sense_key, scsi_defs::asc = scsi_defs::asc::NO_ADDITIONAL_SENSE_INFORMATION,
			scsi_defs::status = scsi_defs::status::CHECK_CONDITION) = 0;
	virtual void Reset() = 0;
	virtual int GetInitiatorId() const = 0;
	virtual void SetByteTransfer(bool) = 0;

	// Get requested LUN based on IDENTIFY message, with LUN from the CDB as fallback
	virtual int GetEffectiveLun() const = 0;

	virtual int GetMaxLuns() const = 0;

	virtual void ScheduleShutdown(rascsi_shutdown_mode) = 0;

	int GetTargetId() const { return target_id; }

	PrimaryDevice *GetDeviceForLun(int) const;
	bool AddDevice(PrimaryDevice *);
	bool DeleteDevice(const PrimaryDevice *);
	bool HasDeviceForLun(int) const;
	int ExtractInitiatorId(int id_data) const;

	// TODO Do not expose internal data
	ctrl_t* GetCtrl() { return &ctrl; }

	int target_id;

protected:

	scsi_defs::scsi_command GetOpcode() const { return (scsi_defs::scsi_command)ctrl.cmd[0]; }

	shared_ptr<BUS> bus;

	ctrl_t ctrl = {};
};

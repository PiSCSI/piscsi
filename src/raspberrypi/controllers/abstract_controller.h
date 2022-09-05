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

	// Internal data definition
	// TODO Some of these data are probably device specific, and in this case they should be moved.
	// These data are not internal, otherwise they could all be private
	using ctrl_t = struct _ctrl_t {
		// General
		BUS::phase_t phase = BUS::busfree;	// Transition phase

		// commands
		DWORD cmd[16];					// Command data
		DWORD status;					// Status data
		int message;					// Message data

		// Transfer
		// TODO Try to get rid of the static buffer
		BYTE *buffer;					// Transfer data buffer
		int bufsize;					// Transfer data buffer size
		uint32_t blocks;				// Number of transfer blocks
		uint64_t next;					// Next record
		uint32_t offset;				// Transfer offset
		uint32_t length;				// Transfer remaining length

		// Logical units of this device controller mapped to their LUN numbers
		unordered_map<int, PrimaryDevice *> luns;
	};

	AbstractController(BUS *bus, int target_id) : bus(bus), target_id(target_id) {}
	~AbstractController() override = default;

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


protected:

	BUS *bus;

	ctrl_t ctrl = {};

private:

	int target_id;
};

//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
// Abstract base class for controllers
//
//---------------------------------------------------------------------------

#pragma once

#include "phase_handler.h"

class PrimaryDevice;

// TODO The abstraction level is not yet high enough, the code is still too SCSI specific
class Controller : virtual public PhaseHandler
{
public:

	// Maximum number of logical units
	static const int LUN_MAX = 32;

	enum rascsi_shutdown_mode {
		NONE,
		STOP_RASCSI,
		STOP_PI,
		RESTART_PI
	};

	// Internal data definition
	// TODO Some of these data are probably device specific, and in this case they should be moved.
	// These data are not internal, otherwise they could all be private
	typedef struct {
		// General
		BUS::phase_t phase;				// Transition phase
		int scsi_id;					// The SCSI ID of the respective device

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

		// Logical units of the device connected to this controller
		PrimaryDevice *units[LUN_MAX];

		// TODO This is probably obsolete
		PrimaryDevice *current_device;
	} ctrl_t;

	Controller(BUS *bus, int scsi_id) {
		this->bus = bus;
		ctrl.scsi_id = scsi_id;
	}
	virtual ~Controller() {}

	virtual BUS::phase_t Process(int) = 0;

	virtual void Error(scsi_defs::sense_key, scsi_defs::asc = scsi_defs::asc::NO_ADDITIONAL_SENSE_INFORMATION,
			scsi_defs::status = scsi_defs::status::CHECK_CONDITION) = 0;
	virtual void Reset() = 0;
	virtual int GetInitiatorId() const = 0;
	virtual PrimaryDevice *GetUnit(int) const = 0;
	virtual void SetLun(int, PrimaryDevice *) = 0;
	virtual void SetByteTransfer(bool) = 0;

	// Get LUN based on IDENTIFY message, with LUN from the CDB as fallback
	virtual int GetEffectiveLun() const = 0;

	virtual void ScheduleShutDown(rascsi_shutdown_mode) = 0;

	// TODO Do not expose internal data
	ctrl_t* GetCtrl() { return &ctrl; }

protected:
	BUS *bus;

	ctrl_t ctrl;
};

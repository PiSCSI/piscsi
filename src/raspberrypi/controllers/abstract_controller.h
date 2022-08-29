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
#include <vector>
#include <unordered_map>

using namespace std;

class PrimaryDevice;

class AbstractController : virtual public PhaseHandler
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
	} ctrl_t;

	AbstractController(BUS *, int);
	virtual ~AbstractController() {}

	virtual BUS::phase_t Process(int) = 0;

	virtual void Error(scsi_defs::sense_key, scsi_defs::asc = scsi_defs::asc::NO_ADDITIONAL_SENSE_INFORMATION,
			scsi_defs::status = scsi_defs::status::CHECK_CONDITION) = 0;
	virtual void Reset() = 0;
	virtual int GetInitiatorId() const = 0;
	virtual void SetByteTransfer(bool) = 0;

	// Get requested LUN based on IDENTIFY message, with LUN from the CDB as fallback
	virtual int GetEffectiveLun() const = 0;

	virtual void ScheduleShutDown(rascsi_shutdown_mode) = 0;

	int GetDeviceId() const { return scsi_id; }

	PrimaryDevice *GetDeviceForLun(int) const;
	void SetDeviceForLun(int, PrimaryDevice *);
	bool HasDeviceForLun(int) const;

	// TODO Do not expose internal data
	ctrl_t* GetCtrl() { return &ctrl; }

	static const vector<AbstractController *> FindAll();
	static AbstractController *FindController(int);
	static void ClearLuns();
	static void DeleteAll();
	static void ResetAll();

protected:

	BUS *bus;

	int scsi_id;

	ctrl_t ctrl = {};

private:

	static unordered_map<int, AbstractController *> controllers;
};

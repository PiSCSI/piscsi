//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//	Copyright (C) akuker
//
//	Licensed under the BSD 3-Clause License. 
//	See LICENSE file in the project root folder.
//
//	[ SCSI device controller ]
//
//---------------------------------------------------------------------------
#pragma once

#include "os.h"
#include "scsi.h"
#include "phase_handler.h"

class PrimaryDevice;

class Controller : virtual public PhaseHandler
{
	friend class HostServices;

	// For timing adjustments
	const unsigned int MIN_EXEC_TIME = 50;

	const int UNKNOWN_SCSI_ID = -1;

	enum rw_command : int {
		eCmdRead6 = 0x08,
		eCmdWrite6 = 0x0A,
		eCmdSetMcastAddr  = 0x0D,    // DaynaPort specific command
		eCmdModeSelect6 = 0x15,
		eCmdRead10 = 0x28,
		eCmdWrite10 = 0x2A,
		eCmdVerify10 = 0x2E,
		eCmdVerify = 0x2F,
		eCmdModeSelect10 = 0x55,
		eCmdRead16 = 0x88,
		eCmdWrite16 = 0x8A,
		eCmdVerify16 = 0x8F,
		eCmdWriteLong10 = 0x3F,
		eCmdWriteLong16 = 0x9F
	};

	typedef struct {
		// Synchronous transfer
		bool syncenable;				// Synchronous transfer possible
		int syncperiod;					// Synchronous transfer period
		int syncoffset;					// Synchronous transfer offset
		int syncack;					// Number of synchronous transfer ACKs

		// ATN message
		bool atnmsg;
		int msc;
		BYTE msb[256];

		// -1 means that the initiator ID is unknown, e.g. with Atari ACSI and old host adapters
		int initiator_id;

		bool is_byte_transfer;
		uint32_t bytes_to_transfer;
	} scsi_t;

	enum rascsi_shutdown_mode {
		NONE,
		STOP_RASCSI,
		STOP_PI,
		RESTART_PI
	};

public:

	// Maximum number of logical units
	static const int UNIT_MAX = 32;

	const int DEFAULT_BUFFER_SIZE = 0x1000;

	// Internal data definition
	// TODO Some of these data are probably device specific, and in this case they should be moved.
	// These data are not internal, otherwise they could all be private
	typedef struct {
		// General
		BUS::phase_t phase;				// Transition phase
		int scsi_id;					// The SCSI ID of the connected device (0-7)

		// commands
		DWORD cmd[16];					// Command data
		DWORD status;					// Status data
		int message;					// Message data

		// Transfer
		// TODO Try to get rid of the static buffer
		BYTE *buffer;					// Transfer data buffer
		int bufsize;					// Transfer data buffer size
		uint32_t blocks;				// Number of transfer block
		uint32_t next;					// Next record
		uint32_t offset;				// Transfer offset
		uint32_t length;				// Transfer remaining length

		// Logical units of the device connected to this controller
		PrimaryDevice *units[UNIT_MAX];

		// The current device
		// TODO This is probably obsolete
		PrimaryDevice *device;
	} ctrl_t;

	Controller(int, BUS *);
	~Controller();

	void Reset();

	BUS::phase_t Process(int);

	// Get LUN based on IDENTIFY message, with LUN from the CDB as fallback
	int GetEffectiveLun() const;

	// TODO Check whether ABORTED_COMMAND is a better default than NO_SENSE
	void Error(scsi_defs::sense_key sense_key = scsi_defs::sense_key::NO_SENSE,
			scsi_defs::asc asc = scsi_defs::asc::NO_ADDITIONAL_SENSE_INFORMATION,
			scsi_defs::status status = scsi_defs::status::CHECK_CONDITION);

	int GetInitiatorId() const { return scsi.initiator_id; }
	void SetByteTransfer(bool is_byte_transfer) { scsi.is_byte_transfer = is_byte_transfer; }

	PrimaryDevice *GetUnit(int lun) const { return ctrl.units[lun]; }
	void SetUnit(int, PrimaryDevice *);

	void Status() override;
	void DataIn() override;
	void DataOut() override;

	// TODO Do not expose internal data
	ctrl_t* GetCtrl() { return &ctrl; }

private:

	BUS *bus;

	// Execution start time
	DWORD execstart;

	// The LUN from the IDENTIFY message
	int identified_lun;

	// Phases
	void BusFree() override;
	void Selection() override;
	void Command() override;
	void MsgIn() override;
	void MsgOut() override;

	// Data transfer
	void Send();
	bool XferMsg(int);
	bool XferIn(BYTE* buf);
	bool XferOut(bool);
	bool XferOutBlockOriented(bool);
	void ReceiveBytes();

	void Execute();
	void FlushUnit();
	void Receive();
	bool HasAnyUnit() const;

	void ScheduleShutDown(rascsi_shutdown_mode shutdown_mode) { this->shutdown_mode = shutdown_mode; }

	void Sleep();

	ctrl_t ctrl;
	scsi_t scsi;

	rascsi_shutdown_mode shutdown_mode;
};


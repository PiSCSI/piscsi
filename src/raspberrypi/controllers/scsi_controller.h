//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
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

#include "abstract_controller.h"
#include "os.h"
#include "scsi.h"

class PrimaryDevice;

class ScsiController : public AbstractController
{
	// For timing adjustments
	static const unsigned int MIN_EXEC_TIME = 50;

	// Transfer period factor (limited to 50 x 4 = 200ns)
	static const int MAX_SYNC_PERIOD = 50;

	// REQ/ACK offset(limited to 16)
	static const int MAX_SYNC_OFFSET = 16;

	static const int UNKNOWN_INITIATOR_ID = -1;

	const int DEFAULT_BUFFER_SIZE = 0x1000;

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
		int syncperiod = MAX_SYNC_PERIOD;	// Synchronous transfer period
		int syncoffset;					// Synchronous transfer offset
		int syncack;					// Number of synchronous transfer ACKs

		// ATN message
		bool atnmsg;
		int msc;
		BYTE msb[256];
	} scsi_t;

public:

	ScsiController(BUS *, int);
	~ScsiController();

	void Reset() override;

	BUS::phase_t Process(int) override;

	int GetEffectiveLun() const override;

	int GetMaxLuns() const override { return LUN_MAX; };

	void Error(scsi_defs::sense_key sense_key, scsi_defs::asc asc = scsi_defs::asc::NO_ADDITIONAL_SENSE_INFORMATION,
			scsi_defs::status status = scsi_defs::status::CHECK_CONDITION);

	int GetInitiatorId() const override { return initiator_id; }
	void SetByteTransfer(bool is_byte_transfer) override { this->is_byte_transfer = is_byte_transfer; }

	void Status() override;
	void DataIn() override;
	void DataOut() override;

private:

	// Execution start time
	DWORD execstart = 0;

	// The initiator ID may be unavailable, e.g. with Atari ACSI and old host adapters
	int initiator_id = UNKNOWN_INITIATOR_ID;

	// The LUN from the IDENTIFY message
	int identified_lun = -1;

	bool is_byte_transfer = false;
	uint32_t bytes_to_transfer = 0;

	// Phases
	void SetPhase(BUS::phase_t phase) override { ctrl.phase = phase; }
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

	void ScheduleShutDown(rascsi_shutdown_mode shutdown_mode) override { this->shutdown_mode = shutdown_mode; }

	void Sleep();

	scsi_t scsi = {};

	rascsi_shutdown_mode shutdown_mode = NONE;
};


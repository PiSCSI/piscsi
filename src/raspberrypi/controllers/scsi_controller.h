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
#include <array>

class PrimaryDevice;

class ScsiController : public AbstractController
{
	// For timing adjustments
	static const unsigned int MIN_EXEC_TIME = 50;

	// Transfer period factor (limited to 50 x 4 = 200ns)
	static const int MAX_SYNC_PERIOD = 50;

	// REQ/ACK offset(limited to 16)
	static const BYTE MAX_SYNC_OFFSET = 16;

	static const int UNKNOWN_INITIATOR_ID = -1;

	const int DEFAULT_BUFFER_SIZE = 0x1000;

	using scsi_t = struct _scsi_t {
		// Synchronous transfer
		bool syncenable;				// Synchronous transfer possible
		BYTE syncperiod = MAX_SYNC_PERIOD;	// Synchronous transfer period
		BYTE syncoffset;					// Synchronous transfer offset
		int syncack;					// Number of synchronous transfer ACKs

		// ATN message
		bool atnmsg;
		int msc;
		std::array<BYTE, 256> msb;
	};

public:

	ScsiController(shared_ptr<BUS>, int);
	~ScsiController () override;
	ScsiController(ScsiController&) = delete;
	ScsiController& operator=(const ScsiController&) = delete;

	void Reset() override;

	BUS::phase_t Process(int) override;

	int GetEffectiveLun() const override;

	int GetMaxLuns() const override { return LUN_MAX; }

	void Error(scsi_defs::sense_key sense_key, scsi_defs::asc asc = scsi_defs::asc::NO_ADDITIONAL_SENSE_INFORMATION,
			scsi_defs::status status = scsi_defs::status::CHECK_CONDITION) override;

	int GetInitiatorId() const override { return initiator_id; }
	void SetByteTransfer(bool b) override { is_byte_transfer = b; }

	void Status() override;
	void DataIn() override;
	void DataOut() override;

private:

	// Execution start time
	uint32_t execstart = 0;

	// The initiator ID may be unavailable, e.g. with Atari ACSI and old host adapters
	int initiator_id = UNKNOWN_INITIATOR_ID;

	// The LUN from the IDENTIFY message
	int identified_lun = -1;

	bool is_byte_transfer = false;
	uint32_t bytes_to_transfer = 0;

	// Phases
	void SetPhase(BUS::phase_t p) override { ctrl.phase = p; }
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

	bool ProcessMessage();

	void ScheduleShutdown(rascsi_shutdown_mode mode) override { shutdown_mode = mode; }

	void Sleep();

	scsi_t scsi = {};

	AbstractController::rascsi_shutdown_mode shutdown_mode = AbstractController::rascsi_shutdown_mode::NONE;
};


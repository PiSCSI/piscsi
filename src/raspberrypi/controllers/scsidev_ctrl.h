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

#include "controllers/sasidev_ctrl.h"

//===========================================================================
//
//	SCSI Device (Interits SASI device)
//
//===========================================================================
class SCSIDEV : public SASIDEV
{
public:

	enum rascsi_shutdown_mode {
		NONE,
		STOP_RASCSI,
		STOP_PI,
		RESTART_PI
	};

	// Internal data definition
	typedef struct {
		// Synchronous transfer
		BOOL syncenable;				// Synchronous transfer possible
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

	SCSIDEV();
	~SCSIDEV();

	void Reset() override;

	BUS::phase_t Process(int) override;

	void Receive() override;

	// Get LUN based on IDENTIFY message, with LUN from the CDB as fallback
	int GetEffectiveLun() const;

	// Common error handling
	void Error(scsi_defs::sense_key sense_key = scsi_defs::sense_key::NO_SENSE,
			scsi_defs::asc asc = scsi_defs::asc::NO_ADDITIONAL_SENSE_INFORMATION,
			scsi_defs::status status = scsi_defs::status::CHECK_CONDITION) override;

	void ScheduleShutDown(rascsi_shutdown_mode shutdown_mode) { this->shutdown_mode = shutdown_mode; }

	int GetInitiatorId() const { return scsi.initiator_id; }
	bool IsByteTransfer() const { return scsi.is_byte_transfer; }
	void SetByteTransfer(bool is_byte_transfer) { scsi.is_byte_transfer = is_byte_transfer; }

	void FlushUnit();

	void Status() override;
	void DataOut() override;

private:
	typedef SASIDEV super;

	// Phases
	void BusFree() override;
	void Selection() override;
	void Command() override;
	void Execute() override;
	void DataIn() override;
	void MsgIn() override;
	void MsgOut();

	// Data transfer
	void Send() override;
	bool XferMsg(int);
	bool XferIn(BYTE* buf);
	bool XferOut(bool);
	bool XferOut2(bool);
	void ReceiveBytes();

	// Internal data
	scsi_t scsi;

	rascsi_shutdown_mode shutdown_mode;
};


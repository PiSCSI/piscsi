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
#include <map>

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
		RASCSI,
		PI
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
	} scsi_t;

	// Basic Functions
	SCSIDEV();
	~SCSIDEV();

	void Reset() override;

	// External API
	BUS::phase_t Process() override;

	void Receive() override;

	// Other
	bool IsSASI() const override { return false; }
	bool IsSCSI() const override { return true; }

	void Error(ERROR_CODES::sense_key sense_key = ERROR_CODES::sense_key::NO_SENSE,
			ERROR_CODES::asc asc = ERROR_CODES::asc::NO_ADDITIONAL_SENSE_INFORMATION,
			ERROR_CODES::status status = ERROR_CODES::status::CHECK_CONDITION) override;	// Common error handling

	void ShutDown(rascsi_shutdown_mode shutdown_mode) { this->shutdown_mode = shutdown_mode; }

private:

	// Phase
	void BusFree() override;						// Bus free phase
	void Selection() override;						// Selection phase
	void Execute() override;						// Execution phase
	void MsgOut();							// Message out phase

	// commands
	void CmdGetEventStatusNotification();
	void CmdModeSelect10();
	void CmdModeSense10();

	// Data transfer
	void Send() override;
	bool XferMsg(DWORD msg);
	bool XferOut(bool);
	void ReceiveBytes();

	scsi_t scsi;								// Internal data

	uint32_t bytes_to_transfer;

	rascsi_shutdown_mode shutdown_mode;
};


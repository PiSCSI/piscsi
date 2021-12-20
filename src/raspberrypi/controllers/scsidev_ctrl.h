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
	enum scsi_command : int {
		eCmdTestUnitReady = 0x00,
		eCmdRezero =  0x01,
		eCmdRequestSense = 0x03,
		eCmdFormat = 0x04,
		eCmdReassign = 0x07,
		eCmdRead6 = 0x08,
		eCmdRetrieveStats = 0x09,    // DaynaPort specific command
		eCmdWrite6 = 0x0A,
		eCmdSeek6 = 0x0B,
		eCmdSetIfaceMode = 0x0C,     // DaynaPort specific command
		eCmdSetMcastAddr  = 0x0D,    // DaynaPort specific command
		eCmdEnableInterface = 0x0E,  // DaynaPort specific command
		eCmdInquiry = 0x12,
		eCmdModeSelect6 = 0x15,
		eCmdReserve6 = 0x16,
		eCmdRelease6 = 0x17,
		eCmdModeSense6 = 0x1A,
		eCmdStartStop = 0x1B,
		eCmdSendDiag = 0x1D,
		eCmdRemoval = 0x1E,
		eCmdReadCapacity10 = 0x25,
		eCmdRead10 = 0x28,
		eCmdWrite10 = 0x2A,
		eCmdSeek10 = 0x2B,
		eCmdVerify10 = 0x2F,
		eCmdSynchronizeCache10 = 0x35,
		eCmdReadDefectData10 = 0x37,
		eCmdReadLong10 = 0x3E,
		eCmdWriteLong10 = 0x3F,
		eCmdReadToc = 0x43,
		eCmdGetEventStatusNotification = 0x4A,
		eCmdModeSelect10 = 0x55,
		eCmdReserve10 = 0x56,
		eCmdRelease10 = 0x57,
		eCmdModeSense10 = 0x5A,
		eCmdRead16 = 0x88,
		eCmdWrite16 = 0x8A,
		eCmdVerify16 = 0x8F,
		eCmdSynchronizeCache16 = 0x91,
		eCmdReadCapacity16_ReadLong16 = 0x9E,
		eCmdWriteLong16 = 0x9F,
		eCmdReportLuns = 0xA0
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

public:
	// Basic Functions
	SCSIDEV();
	~SCSIDEV();

	void Reset() override;

	// External API
	BUS::phase_t Process() override;

	// Other
	bool IsSASI() const override { return false; }
	bool IsSCSI() const override { return true; }

	void Error(ERROR_CODES::sense_key sense_key = ERROR_CODES::sense_key::NO_SENSE,
			ERROR_CODES::asc asc = ERROR_CODES::asc::NO_ADDITIONAL_SENSE_INFORMATION) override;	// Common error handling

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
	void Receive() override;
	bool XferMsg(DWORD msg);

	scsi_t scsi;								// Internal data
};


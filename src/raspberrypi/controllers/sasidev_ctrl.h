//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//  	Copyright (C) akuker
//
//  	Licensed under the BSD 3-Clause License. 
//  	See LICENSE file in the project root folder.
//
//  	[ SASI device controller ]
//
//---------------------------------------------------------------------------
#pragma once

#include "../config.h"
#include "os.h"
#include "scsi.h"
#include "fileio.h"

class PrimaryDevice;

//===========================================================================
//
//	SASI Controller
//
//===========================================================================
class SASIDEV
{
protected:

	enum sasi_command : int {
		eCmdReadCapacity = 0x05,
		eCmdRead6 = 0x08,
		eCmdWrite6 = 0x0A,
		eCmdSeek6 = 0x0B,
		eCmdSetMcastAddr  = 0x0D,    // DaynaPort specific command
		eCmdInquiry = 0x12,
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

public:
	enum {
		UnitMax = 32					// Maximum number of logical units
	};

	const int UNKNOWN_SCSI_ID = -1;
	const int DEFAULT_BUFFER_SIZE = 0x1000;

	// For timing adjustments
	enum {			
		min_exec_time_scsi	= 50
	};

	// Internal data definition
	typedef struct {
		// General
		BUS::phase_t phase;				// Transition phase
		int m_scsi_id;					// Controller ID (0-7)
		BUS *bus;						// Bus

		// commands
		DWORD cmd[16];					// Command data
		DWORD status;					// Status data
		DWORD message;					// Message data

		// Run
		DWORD execstart;				// Execution start time

		// Transfer
		BYTE *buffer;					// Transfer data buffer
		int bufsize;					// Transfer data buffer size
		uint32_t blocks;				// Number of transfer block
		DWORD next;						// Next record
		DWORD offset;					// Transfer offset
		DWORD length;					// Transfer remaining length

		// Logical units
		PrimaryDevice *unit[UnitMax];

		// The current device
		PrimaryDevice *device;

		// The LUN from the IDENTIFY message
		int lun;
	} ctrl_t;

public:
	// Basic Functions
	SASIDEV();
	virtual ~SASIDEV();							// Destructor
	virtual void Reset();						// Device Reset

	// External API
	virtual BUS::phase_t Process(int);				// Run

	// Connect
	void Connect(int id, BUS *sbus);				// Controller connection
	PrimaryDevice* GetUnit(int no);							// Get logical unit
	void SetUnit(int no, PrimaryDevice *dev);				// Logical unit setting
	bool HasUnit();						// Has a valid logical unit

	// Other
	BUS::phase_t GetPhase() {return ctrl.phase;}			// Get the phase

	int GetSCSIID() {return ctrl.m_scsi_id;}					// Get the ID
	ctrl_t* GetCtrl() { return &ctrl; }			// Get the internal information address

public:
	virtual void DataIn();							// Data in phase
	virtual void Status();							// Status phase
	virtual void MsgIn();							// Message in phase
	virtual void DataOut();						// Data out phase

	virtual int GetEffectiveLun() const;

	virtual void Error(scsi_defs::sense_key sense_key = scsi_defs::sense_key::NO_SENSE,
			scsi_defs::asc = scsi_defs::asc::NO_ADDITIONAL_SENSE_INFORMATION,
			scsi_defs::status = scsi_defs::status::CHECK_CONDITION);	// Common error handling

protected:
	// Phase processing
	virtual void BusFree();					// Bus free phase
	virtual void Selection();					// Selection phase
	virtual void Command();					// Command phase
	virtual void Execute();					// Execution phase

	// Data transfer
	virtual void Send();						// Send data
	virtual void Receive();					// Receive data

	ctrl_t ctrl;								// Internal data
};

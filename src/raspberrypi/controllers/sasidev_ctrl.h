//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//  Copyright (C) akuker
//
//  Licensed under the BSD 3-Clause License. 
//  See LICENSE file in the project root folder.
//
//  [ SASI device controller ]
//
//---------------------------------------------------------------------------
#pragma once

#include "os.h"
#include "scsi.h"
#include "fileio.h"
#include "devices/disk.h"
#include "log.h"
#include "xm6.h"


//===========================================================================
//
//	SASI Controller
//
//===========================================================================
class SASIDEV
{
public:
	// Maximum number of logical units
	enum {
		UnitMax = 8
	};

#ifdef RASCSI
	// For timing adjustments
	enum {
		min_exec_time_sasi	= 100,			// SASI BOOT/FORMAT 30:NG 35:OK
		min_exec_time_scsi	= 50
	};
#endif	// RASCSI

	// Internal data definition
	typedef struct {
		// 全般
		BUS::phase_t phase;				// Transition phase
		int id;							// Controller ID (0-7)
		BUS *bus;						// Bus

		// commands
		DWORD cmd[10];					// Command data
		DWORD status;					// Status data
		DWORD message;					// Message data

#ifdef RASCSI
		// Run
		DWORD execstart;				// Execution start time
#endif	// RASCSI

		// Transfer
		BYTE *buffer;					// Transfer data buffer
		int bufsize;					// Transfer data buffer size
		DWORD blocks;					// Number of transfer block
		DWORD next;						// Next record
		DWORD offset;					// Transfer offset
		DWORD length;					// Transfer remaining length

		// Logical unit
		Disk *unit[UnitMax];
										// Logical Unit
	} ctrl_t;

public:
	// Basic Functions
#ifdef RASCSI
	SASIDEV();
#else
	SASIDEV(Device *dev);
#endif //RASCSI

										// Constructor
	virtual ~SASIDEV();
										// Destructor
	virtual void FASTCALL Reset();
										// Device Reset
#ifndef RASCSI
	virtual BOOL FASTCALL Save(Fileio *fio, int ver);
										// Save
	virtual BOOL FASTCALL Load(Fileio *fio, int ver);
										// Load
#endif //RASCSI

	// External API
	virtual BUS::phase_t FASTCALL Process();
										// Run

	// Connect
	void FASTCALL Connect(int id, BUS *sbus);
										// Controller connection
	Disk* FASTCALL GetUnit(int no);
										// Get logical unit
	void FASTCALL SetUnit(int no, Disk *dev);
										// Logical unit setting
	BOOL FASTCALL HasUnit();
										// Has a valid logical unit

	// Other
	BUS::phase_t FASTCALL GetPhase() {return ctrl.phase;}
										// Get the phase
#ifdef DISK_LOG
	// Function to get the current phase as a String.
	void FASTCALL GetPhaseStr(char *str);
#endif

	int FASTCALL GetID() {return ctrl.id;}
										// Get the ID
	void FASTCALL GetCTRL(ctrl_t *buffer);
										// Get the internal information
	ctrl_t* FASTCALL GetWorkAddr() { return &ctrl; }
										// Get the internal information address
	virtual BOOL FASTCALL IsSASI() const {return TRUE;}
										// SASI Check
	virtual BOOL FASTCALL IsSCSI() const {return FALSE;}
										// SCSI check
	Disk* FASTCALL GetBusyUnit();
										// Get the busy unit

protected:
	// Phase processing
	virtual void FASTCALL BusFree();
										// Bus free phase
	virtual void FASTCALL Selection();
										// Selection phase
	virtual void FASTCALL Command();
										// Command phase
	virtual void FASTCALL Execute();
										// Execution phase
	void FASTCALL Status();
										// Status phase
	void FASTCALL MsgIn();
										// Message in phase
	void FASTCALL DataIn();
										// Data in phase
	void FASTCALL DataOut();
										// Data out phase
	virtual void FASTCALL Error();
										// Common error handling

	// commands
	void FASTCALL CmdTestUnitReady();
										// TEST UNIT READY command
	void FASTCALL CmdRezero();
										// REZERO UNIT command
	void FASTCALL CmdRequestSense();
										// REQUEST SENSE command
	void FASTCALL CmdFormat();
										// FORMAT command
	void FASTCALL CmdReassign();
										// REASSIGN BLOCKS command
	void FASTCALL CmdRead6();
										// READ(6) command
	void FASTCALL CmdWrite6();
										// WRITE(6) command
	void FASTCALL CmdSeek6();
										// SEEK(6) command
	void FASTCALL CmdAssign();
										// ASSIGN command
	void FASTCALL CmdSpecify();
										// SPECIFY command
	void FASTCALL CmdInvalid();
										// Unsupported command

	// データ転送
	virtual void FASTCALL Send();
										// Send data
#ifndef RASCSI
	virtual void FASTCALL SendNext();
										// Continue sending data
#endif	// RASCSI
	virtual void FASTCALL Receive();
										// Receive data
#ifndef RASCSI
	virtual void FASTCALL ReceiveNext();
										// Continue receiving data
#endif	// RASCSI
	BOOL FASTCALL XferIn(BYTE* buf);
										// Data transfer IN
	BOOL FASTCALL XferOut(BOOL cont);
										// Data transfer OUT

	// Special operations
	void FASTCALL FlushUnit();
										// Flush the logical unit

	// Log
	void FASTCALL Log(Log::loglevel level, const char *format, ...);
										// Log output

protected:
#ifndef RASCSI
	Device *host;
										// Host device
#endif // RASCSI

	ctrl_t ctrl;
										// Internal data
};

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

#include "os.h"
#include "scsi.h"
#include "fileio.h"
#include "log.h"
#include "xm6.h"


//===========================================================================
//
//	SASI Controller
//
//===========================================================================
class SASIDEV
{
protected:
	enum scsi_message_code : BYTE {
		eMsgCodeAbort                              = 0x06,                           
		eMsgCodeAbortTag                           = 0x0D,
		eMsgCodeBusDeviceReset                   = 0x0C,
		eMsgCodeClearQueue             = 0x0E,
		eMsgCodeCommandComplete                   = 0x00,
		eMsgCodeDisconnect                         = 0x04,
		eMsgCodeIdentify                           = 0x80,
		eMsgCodeIgnoreWideResidue    = 0x23, // (Two Bytes)
		eMsgCodeInitiateRecovery                  = 0x0F,
		eMsgCodeInitiatorDetectedError           = 0x05,
		eMsgCodeLinkedCommandComplete            = 0x0A,
		eMsgCodeLinkedCommandCompleteWithFlag = 0x0B,
		eMsgCodeMessageParityError               = 0x09,
		eMsgCodeMessageReject                     = 0x07,
		eMsgCodeNoOperation                       = 0x08,
		eMsgCodeHeadOfQueueTag                = 0x21,
		eMsgCodeOrderedQueueTag                = 0x22,
		eMsgCodeSimpleQueueTag                 = 0x20,
		eMsgCodeReleaseRecovery                   = 0x10,
		eMsgCodeRestorePointers                   = 0x03,
		eMsgCodeSaveDataPointer                  = 0x02,
		eMsgCodeTerminateIOProcess              = 0x11,
	};

private:
	enum scsi_command : BYTE {
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
			eCmdModeSelect = 0x15,
			eCmdReserve6 = 0x16,
			eCmdRelease6 = 0x17,
			eCmdModeSense = 0x1A,
			eCmdStartStop = 0x1B,
			eCmdSendDiag = 0x1D,
			eCmdRemoval = 0x1E,
			eCmdReadCapacity = 0x25,
			eCmdRead10 = 0x28,
			eCmdWrite10 = 0x2A,
			eCmdSeek10 = 0x2B,
			eCmdWriteAndVerify10 = 0x2E,
			eCmdVerify = 0x2F,
			eCmdSynchronizeCache = 0x35,
			eCmdReadDefectData10 = 0x37,
			eCmdReadToc = 0x43,
			eCmdPlayAudio10 = 0x45,
			eCmdPlayAudioMSF = 0x47,
			eCmdPlayAudioTrack = 0x48,
			eCmdModeSelect10 = 0x55,
			eCmdReserve10 = 0x56,
			eCmdRelease10 = 0x57,
			eCmdModeSense10 = 0x5A,
			eCmdInvalid = 0xC2,		// (SASI only/Suppress warning when using SxSI)
			eCmdSasiCmdAssign = 0x0e, // This isn't used by SCSI, and can probably be removed.
	};

public:
	enum {
		UnitMax = 8					// Maximum number of logical units		
	};

	const int UNKNOWN_SCSI_ID = -1;
	const int DEFAULT_BUFFER_SIZE = 0x800;
	const int DAYNAPORT_BUFFER_SIZE = 0x1000000;

	#ifdef RASCSI
	// For timing adjustments
	enum {			
		min_exec_time_sasi	= 100,			// SASI BOOT/FORMAT 30:NG 35:OK
		min_exec_time_scsi	= 50
	};
	#endif	// RASCSI

	// Internal data definition
	typedef struct {
		// General
		BUS::phase_t phase;				// Transition phase
		int m_scsi_id;							// Controller ID (0-7)
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
		DWORD next;					// Next record
		DWORD offset;					// Transfer offset
		DWORD length;					// Transfer remaining length

		// Logical unit
		Disk *unit[UnitMax];				// Logical Unit
	} ctrl_t;

public:
	// Basic Functions
	#ifdef RASCSI
	SASIDEV();
	#else

	SASIDEV(Device *dev);							// Constructor

	#endif //RASCSI
	virtual ~SASIDEV();							// Destructor
	virtual void FASTCALL Reset();						// Device Reset

	#ifndef RASCSI
	virtual BOOL FASTCALL Save(Fileio *fio, int ver);			// Save
	virtual BOOL FASTCALL Load(Fileio *fio, int ver);			// Load
	#endif //RASCSI

	// External API
	virtual BUS::phase_t FASTCALL Process();				// Run

	// Connect
	void FASTCALL Connect(int id, BUS *sbus);				// Controller connection
	Disk* FASTCALL GetUnit(int no);						// Get logical unit
	void FASTCALL SetUnit(int no, Disk *dev);				// Logical unit setting
	BOOL FASTCALL HasUnit();						// Has a valid logical unit

	// Other
	BUS::phase_t FASTCALL GetPhase() {return ctrl.phase;}			// Get the phase
	#ifdef DISK_LOG

	// Function to get the current phase as a String.
	void FASTCALL GetPhaseStr(char *str);			
	#endif

	int FASTCALL GetSCSIID() {return ctrl.m_scsi_id;}					// Get the ID
	void FASTCALL GetCTRL(ctrl_t *buffer);					// Get the internal information
	ctrl_t* FASTCALL GetWorkAddr() { return &ctrl; }			// Get the internal information address
	virtual BOOL FASTCALL IsSASI() const {return TRUE;}			// SASI Check
	virtual BOOL FASTCALL IsSCSI() const {return FALSE;}			// SCSI check
	Disk* FASTCALL GetBusyUnit();						// Get the busy unit

protected:
	// Phase processing
	virtual void FASTCALL BusFree();					// Bus free phase
	virtual void FASTCALL Selection();					// Selection phase
	virtual void FASTCALL Command();					// Command phase
	virtual void FASTCALL Execute();					// Execution phase
	void FASTCALL Status();							// Status phase
	void FASTCALL MsgIn();							// Message in phase
	void FASTCALL DataIn();							// Data in phase
	void FASTCALL DataOut();						// Data out phase
	virtual void FASTCALL Error();						// Common error handling

	// commands
	void FASTCALL CmdTestUnitReady();					// TEST UNIT READY command
	void FASTCALL CmdRezero();						// REZERO UNIT command
	void FASTCALL CmdRequestSense();					// REQUEST SENSE command
	void FASTCALL CmdFormat();						// FORMAT command
	void FASTCALL CmdReassign();						// REASSIGN BLOCKS command
	void FASTCALL CmdReserveUnit();						// RESERVE UNIT command
	void FASTCALL CmdReleaseUnit();						// RELEASE UNIT command
	void FASTCALL CmdRead6();						// READ(6) command
	void FASTCALL CmdWrite6();						// WRITE(6) command
	void FASTCALL CmdSeek6();						// SEEK(6) command
	void FASTCALL CmdAssign();						// ASSIGN command
	void FASTCALL CmdSpecify();						// SPECIFY command
	void FASTCALL CmdInvalid();						// Unsupported command
	void FASTCALL DaynaPortWrite();					// DaynaPort specific 'write' operation
	// データ転送
	virtual void FASTCALL Send();						// Send data

	#ifndef RASCSI
	virtual void FASTCALL SendNext();					// Continue sending data
	#endif	// RASCSI

	virtual void FASTCALL Receive();					// Receive data

	#ifndef RASCSI
	virtual void FASTCALL ReceiveNext();					// Continue receiving data
	#endif	// RASCSI

	BOOL FASTCALL XferIn(BYTE* buf);					// Data transfer IN
	BOOL FASTCALL XferOut(BOOL cont);					// Data transfer OUT

	// Special operations
	void FASTCALL FlushUnit();						// Flush the logical unit

        DWORD FASTCALL GetLun();                                           // Get the validated LUN

protected:
	#ifndef RASCSI
	Device *host;								// Host device
	#endif // RASCSI
	ctrl_t ctrl;								// Internal data
};

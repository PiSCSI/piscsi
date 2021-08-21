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

protected:
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
			eCmdModeSelect = 0x15,
			eCmdReserve6 = 0x16,
			eCmdRelease6 = 0x17,
			eCmdModeSense = 0x1A,
			eCmdStartStop = 0x1B,
			eCmdSendDiag = 0x1D,
			eCmdRemoval = 0x1E,
			eCmdReadCapacity10 = 0x25,
			eCmdRead10 = 0x28,
			eCmdWrite10 = 0x2A,
			eCmdSeek10 = 0x2B,
			eCmdVerify10 = 0x2E,
			eCmdVerify = 0x2F,
			eCmdSynchronizeCache = 0x35,
			eCmdReadDefectData10 = 0x37,
			eCmdReadToc = 0x43,
			eCmdPlayAudio10 = 0x45,
			eCmdPlayAudioMSF = 0x47,
			eCmdPlayAudioTrack = 0x48,
			eCmdGetEventStatusNotification = 0x4a,
			eCmdModeSelect10 = 0x55,
			eCmdReserve10 = 0x56,
			eCmdRelease10 = 0x57,
			eCmdModeSense10 = 0x5A,
			eCmdRead16 = 0x88,
			eCmdWrite16 = 0x8A,
			eCmdVerify16 = 0x8F,
			eCmdReadCapacity16 = 0x9E,
			eCmdReportLuns = 0xA0,
			eCmdInvalid = 0xC2,		// (SASI only/Suppress warning when using SxSI)
			eCmdSasiCmdAssign = 0x0E, // This isn't used by SCSI, and can probably be removed.
	};

public:
	enum {
		UnitMax = 8					// Maximum number of logical units		
	};

	const int UNKNOWN_SCSI_ID = -1;
	const int DEFAULT_BUFFER_SIZE = 0x1000;
	const int DAYNAPORT_BUFFER_SIZE = 0x1000000;

	// For timing adjustments
	enum {			
		min_exec_time_sasi	= 100,			// SASI BOOT/FORMAT 30:NG 35:OK
		min_exec_time_scsi	= 50
	};

	// Internal data definition
	typedef struct {
		// General
		BUS::phase_t phase;				// Transition phase
		int m_scsi_id;							// Controller ID (0-7)
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
		DWORD blocks;					// Number of transfer block
		DWORD next;						// Next record
		DWORD offset;					// Transfer offset
		DWORD length;					// Transfer remaining length

		// Logical unit
		Disk *unit[UnitMax];
	} ctrl_t;

public:
	// Basic Functions
	SASIDEV();
	virtual ~SASIDEV();							// Destructor
	virtual void Reset();						// Device Reset

	// External API
	virtual BUS::phase_t Process();				// Run

	// Connect
	void Connect(int id, BUS *sbus);				// Controller connection
	Disk* GetUnit(int no);							// Get logical unit
	void SetUnit(int no, Disk *dev);				// Logical unit setting
	BOOL HasUnit();						// Has a valid logical unit

	// Other
	BUS::phase_t GetPhase() {return ctrl.phase;}			// Get the phase
	#ifdef DISK_LOG

	// Function to get the current phase as a String.
	void GetPhaseStr(char *str);			
	#endif

	int GetSCSIID() {return ctrl.m_scsi_id;}					// Get the ID
	void GetCTRL(ctrl_t *buffer);					// Get the internal information
	ctrl_t* GetWorkAddr() { return &ctrl; }			// Get the internal information address
	virtual BOOL IsSASI() const {return TRUE;}			// SASI Check
	virtual BOOL IsSCSI() const {return FALSE;}			// SCSI check
	Disk* GetBusyUnit();								// Get the busy unit

protected:
	// Phase processing
	virtual void BusFree();					// Bus free phase
	virtual void Selection();					// Selection phase
	virtual void Command();					// Command phase
	virtual void Execute();					// Execution phase
	void Status();							// Status phase
	void MsgIn();							// Message in phase
	void DataIn();							// Data in phase
	void DataOut();						// Data out phase
	virtual void Error(ERROR_CODES::sense_key sense_key = ERROR_CODES::sense_key::NO_SENSE,
			ERROR_CODES::asc = ERROR_CODES::asc::NO_ADDITIONAL_SENSE_INFORMATION);	// Common error handling

	// commands
	void CmdTestUnitReady();					// TEST UNIT READY command
	void CmdRezero();						// REZERO UNIT command
	void CmdRequestSense();					// REQUEST SENSE command
	void CmdFormat();						// FORMAT command
	void CmdReassign();						// REASSIGN BLOCKS command
	void CmdReserveUnit();						// RESERVE UNIT command
	void CmdReleaseUnit();						// RELEASE UNIT command
	void CmdRead6();						// READ(6) command
	void CmdWrite6();						// WRITE(6) command
	void CmdSeek6();						// SEEK(6) command
	void CmdAssign();						// ASSIGN command
	void CmdSpecify();						// SPECIFY command
	void CmdInvalid();						// Unsupported command
	void DaynaPortWrite();					// DaynaPort specific 'write' operation
	// データ転送
	virtual void Send();						// Send data

	virtual void Receive();					// Receive data

	BOOL XferIn(BYTE* buf);					// Data transfer IN
	BOOL XferOut(BOOL cont);					// Data transfer OUT

	// Special operations
	void FlushUnit();						// Flush the logical unit

        DWORD GetLun();                                           // Get the validated LUN

protected:
	ctrl_t ctrl;								// Internal data
};

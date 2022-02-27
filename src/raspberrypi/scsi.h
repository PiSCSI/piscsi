//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//
//	[ SCSI Common Functionality ]
//
//---------------------------------------------------------------------------

#pragma once
#include "os.h"

//===========================================================================
//
//	Status byte codes, Sense Keys and Additional Sense Codes
//  (See https://www.t10.org/lists/1spc-lst.htm)
//
//===========================================================================
class ERROR_CODES
{
public:
	enum status : int {
		GOOD = 0x00,
		CHECK_CONDITION = 0x02,
		CONDITION_MET = 0x04,
		BUSY = 0x08,
		INTERMEDIATE = 0x10,
		INTERMEDIATE_CONDITION_MET = 0x14,
		RESERVATION_CONFLICT = 0x18,
		COMMAND_TERMINATED = 0x22,
		QUEUE_FULL = 0x28
	};

	enum sense_key : int {
		NO_SENSE = 0x00,
		RECOVERED_ERROR = 0x01,
		NOT_READY = 0x02,
		MEDIUM_ERROR = 0x03,
		HARDWARE_ERROR = 0x04,
		ILLEGAL_REQUEST = 0x05,
		UNIT_ATTENTION = 0x06,
		DATA_PROTECT = 0x07,
		BLANK_CHECK = 0x08,
		VENDOR_SPECIFIC = 0x09,
		COPY_ABORTED = 0x0a,
		ABORTED_COMMAND = 0x0b,
		VOLUME_OVERFLOW = 0x0d,
		MISCOMPARE = 0x0e,
		COMPLETED = 0x0f
	};

	enum asc : int {
		NO_ADDITIONAL_SENSE_INFORMATION = 0x00,
		INVALID_COMMAND_OPERATION_CODE = 0x20,
		LBA_OUT_OF_RANGE = 0x21,
		INVALID_FIELD_IN_CDB = 0x24,
		INVALID_LUN = 0x25,
		WRITE_PROTECTED = 0x27,
		NOT_READY_TO_READY_CHANGE = 0x28,
		MEDIUM_NOT_PRESENT = 0x3a
	};
};

//===========================================================================
//
//	SASI/SCSI Bus
//
//===========================================================================
class BUS
{
public:
	// Operation modes definition
	enum mode_e {
		TARGET = 0,
		INITIATOR = 1,
		MONITOR = 2,
	};

	//	Phase definitions
	enum phase_t : BYTE {
		busfree,
		arbitration,
		selection,
		reselection,
		command,
		execute,						// Execute phase is an extension of the command phase
		datain,
		dataout,
		status,
		msgin,
		msgout,
		reserved						// Unused
	};

	BUS() { };
	virtual ~BUS() { };

	// Basic Functions
	virtual BOOL Init(mode_e mode) = 0;
	virtual void Reset() = 0;
	virtual void Cleanup() = 0;
	phase_t GetPhase();

	static phase_t GetPhase(DWORD mci)
	{
		return phase_table[mci];
	}

	static const char* GetPhaseStrRaw(phase_t current_phase);
										// Get the string phase name, based upon the raw data

	// Extract as specific pin field from a raw data capture
	static inline DWORD GetPinRaw(DWORD raw_data, DWORD pin_num)
	{
		return ((raw_data >> pin_num) & 1);
	}

	virtual bool GetBSY() = 0;
	virtual void SetBSY(bool ast) = 0;

	virtual BOOL GetSEL() = 0;
	virtual void SetSEL(BOOL ast) = 0;

	virtual BOOL GetATN() = 0;
	virtual void SetATN(BOOL ast) = 0;

	virtual BOOL GetACK() = 0;
	virtual void SetACK(BOOL ast) = 0;

	virtual BOOL GetRST() = 0;
	virtual void SetRST(BOOL ast) = 0;

	virtual BOOL GetMSG() = 0;
	virtual void SetMSG(BOOL ast) = 0;

	virtual BOOL GetCD() = 0;
	virtual void SetCD(BOOL ast) = 0;

	virtual BOOL GetIO() = 0;
	virtual void SetIO(BOOL ast) = 0;

	virtual BOOL GetREQ() = 0;
	virtual void SetREQ(BOOL ast) = 0;

	virtual BYTE GetDAT() = 0;
	virtual void SetDAT(BYTE dat) = 0;
	virtual BOOL GetDP() = 0;			// Get parity signal

	virtual DWORD Aquire() = 0;
	virtual int CommandHandShake(BYTE *buf) = 0;
	virtual int ReceiveHandShake(BYTE *buf, int count) = 0;
	virtual int SendHandShake(BYTE *buf, int count, int delay_after_bytes) = 0;

	virtual BOOL GetSignal(int pin) = 0;
										// Get SCSI input signal value
	virtual void SetSignal(int pin, BOOL ast) = 0;
										// Set SCSI output signal value
	static const int SEND_NO_DELAY = -1;
										// Passed into SendHandShake when we don't want to delay
protected:
	phase_t m_current_phase = phase_t::reserved;

private:
	static const phase_t phase_table[8];

	static const char* phase_str_table[];
};

namespace scsi_defs {
	enum scsi_command : int {
		eCmdTestUnitReady = 0x00,
		eCmdRezero =  0x01,
		eCmdRequestSense = 0x03,
		eCmdFormat = 0x04,
		eCmdReassign = 0x07,
		eCmdRead6 = 0x08,
		// DaynaPort specific command
		eCmdRetrieveStats = 0x09,
		eCmdWrite6 = 0x0A,
		eCmdSeek6 = 0x0B,
		// DaynaPort specific command
		eCmdSetIfaceMode = 0x0C,
		// DaynaPort specific command
		eCmdSetMcastAddr  = 0x0D,
		// DaynaPort specific command
		eCmdEnableInterface = 0x0E,
		eCmdSynchronizeBuffer = 0x10,
		eCmdInquiry = 0x12,
		eCmdModeSelect6 = 0x15,
		eCmdReserve6 = 0x16,
		eCmdRelease6 = 0x17,
		eCmdModeSense6 = 0x1A,
		eCmdStartStop = 0x1B,
		eCmdSendDiag = 0x1D,
		eCmdRemoval = 0x1E,
		// ICD specific command
		eCmdIcd = 0x1F,
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
};

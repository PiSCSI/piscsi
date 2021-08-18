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
	// Internal data definition
	typedef struct {
		// Synchronous transfer
		BOOL syncenable;				// Synchronous transfer possible
		int syncperiod;					// Synchronous transfer period
		int syncoffset;					// Synchronous transfer offset
		int syncack;					// Number of synchronous transfer ACKs

		// ATN message
		BOOL atnmsg;
		int msc;
		BYTE msb[256];
	} scsi_t;

	// SCSI command name and pointer to implementation
	typedef struct _command_t {
		const char* name;
		void (SCSIDEV::*execute)(void);

		_command_t(const char* _name, void (SCSIDEV::*_execute)(void)) : name(_name), execute(_execute) { };
	} command_t;

	// Mapping of SCSI opcodes to command implementations
	std::map<scsi_command, command_t*> scsi_commands;

public:
	// Basic Functions
	SCSIDEV();
	~SCSIDEV();

	void Reset();							// Device Reset

	// 外部API
	BUS::phase_t Process();					// Run

	void SyncTransfer(BOOL enable) { scsi.syncenable = enable; }	// Synchronouse transfer enable setting

	// Other
	BOOL IsSASI() const {return FALSE;}				// SASI Check
	BOOL IsSCSI() const {return TRUE;}				// SCSI check

private:
	void SetUpCommand(scsi_command, const char*, void (SCSIDEV::*)(void));

	// Phase
	void BusFree();						// Bus free phase
	void Selection();						// Selection phase
	void Execute();						// Execution phase
	void MsgOut();							// Message out phase
	void Error(ERROR_CODES::sense_key sense_key = ERROR_CODES::sense_key::NO_SENSE,
			ERROR_CODES::asc asc = ERROR_CODES::asc::NO_ADDITIONAL_SENSE_INFORMATION);	// Common erorr handling

	// commands
	void CmdInquiry();						// INQUIRY command
	void CmdModeSelect();						// MODE SELECT command
	void CmdReserve6();						// RESERVE(6) command
	void CmdReserve10();						// RESERVE(10) command
	void CmdRelease6();						// RELEASE(6) command
	void CmdRelease10();						// RELEASE(10) command
	void CmdModeSense();						// MODE SENSE command
	void CmdStartStop();						// START STOP UNIT command
	void CmdSendDiag();						// SEND DIAGNOSTIC command
	void CmdRemoval();						// PREVENT/ALLOW MEDIUM REMOVAL command
	void CmdReadCapacity10();					// READ CAPACITY(10) command
	void CmdRead10();						// READ(10) command
	void CmdWrite10();						// WRITE(10) command
	void CmdSeek10();						// SEEK(10) command
	void CmdVerify();						// VERIFY command
	void CmdSynchronizeCache();					// SYNCHRONIZE CACHE  command
	void CmdReadDefectData10();					// READ DEFECT DATA(10)  command
	void CmdReadToc();						// READ TOC command
	void CmdPlayAudio10();						// PLAY AUDIO(10) command
	void CmdPlayAudioMSF();					// PLAY AUDIO MSF command
	void CmdPlayAudioTrack();					// PLAY AUDIO TRACK INDEX command
	void CmdGetEventStatusNotification();
	void CmdModeSelect10();					// MODE SELECT(10) command
	void CmdModeSense10();						// MODE SENSE(10) command
	void CmdReadCapacity16();					// READ CAPACITY(16) command
	void CmdRead16();						// READ(16) command
	void CmdWrite16();						// WRITE(16) command
	void CmdReportLuns();					// REPORT LUNS command
	void CmdGetMessage10();					// GET MESSAGE(10) command
	void CmdSendMessage10();					// SEND MESSAGE(10) command
	void CmdRetrieveStats();   				// DaynaPort specific command
	void CmdSetIfaceMode();    				// DaynaPort specific command
	void CmdSetMcastAddr();					// DaynaPort specific command
	void CmdEnableInterface(); 				// DaynaPort specific command
	// データ転送
	void Send();							// Send data
	void Receive();						// Receive data
	BOOL XferMsg(DWORD msg);					// Data transfer message

	scsi_t scsi;								// Internal data
};


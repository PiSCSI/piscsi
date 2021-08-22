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

private:
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
			eCmdReportLuns = 0xA0
	};

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
	typedef struct _controller_command_t {
		const char* name;
		void (SCSIDEV::*execute)(void);

		_controller_command_t(const char* _name, void (SCSIDEV::*_execute)(void)) : name(_name), execute(_execute) { };
	} controller_command_t;
	std::map<scsi_command, controller_command_t*> controller_commands;

	typedef struct _disk_command_t {
		const char* name;
		void (Disk::*execute)(SASIDEV *);

		_disk_command_t(const char* _name, void (Disk::*_execute)(SASIDEV *)) : name(_name), execute(_execute) { };
	} disk_command_t;
	std::map<scsi_command, disk_command_t*> disk_commands;

public:
	// Basic Functions
	SCSIDEV();
	~SCSIDEV();

	void Reset();							// Device Reset

	// 外部API
	BUS::phase_t Process();					// Run

	// Other
	BOOL IsSASI() const {return FALSE;}				// SASI Check
	BOOL IsSCSI() const {return TRUE;}				// SCSI check

	void Error(ERROR_CODES::sense_key sense_key = ERROR_CODES::sense_key::NO_SENSE,
			ERROR_CODES::asc asc = ERROR_CODES::asc::NO_ADDITIONAL_SENSE_INFORMATION) override;	// Common erorr handling

	void CmdGetMessage10();					// GET MESSAGE(10) command
	void CmdSendMessage10();					// SEND MESSAGE(10) command

private:
	void SetUpControllerCommand(scsi_command, const char*, void (SCSIDEV::*)(void));
	void SetUpDiskCommand(scsi_command, const char*, void (Disk::*)(SASIDEV *));

	// Phase
	void BusFree();						// Bus free phase
	void Selection();						// Selection phase
	void Execute();						// Execution phase
	void MsgOut();							// Message out phase

	// commands
	void CmdReadToc();						// READ TOC command
	void CmdPlayAudio10();						// PLAY AUDIO(10) command
	void CmdPlayAudioMSF();					// PLAY AUDIO MSF command
	void CmdPlayAudioTrack();					// PLAY AUDIO TRACK INDEX command
	void CmdGetEventStatusNotification();
	void CmdModeSelect10();					// MODE SELECT(10) command
	void CmdModeSense10();						// MODE SENSE(10) command
	void CmdRetrieveStats();   				// DaynaPort specific command
	void CmdSetIfaceMode();    				// DaynaPort specific command
	void CmdSetMcastAddr();					// DaynaPort specific command
	void CmdEnableInterface(); 				// DaynaPort specific command
	// データ転送
	void Send();							// Send data
	void Receive();						// Receive data
	BOOL XferMsg(DWORD msg);					// Data transfer message

	DWORD GetLun() const;

	scsi_t scsi;								// Internal data
};


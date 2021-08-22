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
	typedef struct _controller_command_t {
		const char* name;
		void (SCSIDEV::*execute)(void);

		_controller_command_t(const char* _name, void (SCSIDEV::*_execute)(void)) : name(_name), execute(_execute) { };
	} controller_command_t;
	std::map<scsi_command, controller_command_t*> controller_commands;

	typedef struct _device_command_t {
		const char* name;
		void (Disk::*execute)(SASIDEV *);

		_device_command_t(const char* _name, void (Disk::*_execute)(SASIDEV *)) : name(_name), execute(_execute) { };
	} device_command_t;
	std::map<scsi_command, device_command_t*> device_commands;

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

	void Error(ERROR_CODES::sense_key sense_key = ERROR_CODES::sense_key::NO_SENSE,
			ERROR_CODES::asc asc = ERROR_CODES::asc::NO_ADDITIONAL_SENSE_INFORMATION);	// Common erorr handling

	void CmdGetMessage10();					// GET MESSAGE(10) command
	void CmdSendMessage10();					// SEND MESSAGE(10) command

private:
	void SetUpControllerCommand(scsi_command, const char*, void (SCSIDEV::*)(void));
	void SetUpDeviceCommand(scsi_command, const char*, void (Disk::*)(SASIDEV *));

	// Phase
	void BusFree();						// Bus free phase
	void Selection();						// Selection phase
	void Execute();						// Execution phase
	void MsgOut();							// Message out phase

	// commands
	void CmdInquiry();						// INQUIRY command
	void CmdModeSelect();						// MODE SELECT command
	void CmdModeSense();						// MODE SENSE command
	void CmdStartStop();						// START STOP UNIT command
	void CmdSendDiag();						// SEND DIAGNOSTIC command
	void CmdRemoval();						// PREVENT/ALLOW MEDIUM REMOVAL command
	void CmdSynchronizeCache();					// SYNCHRONIZE CACHE  command
	void CmdReadDefectData10();					// READ DEFECT DATA(10)  command
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

	bool GetStartAndCount(uint64_t&, uint32_t&, bool);

	scsi_t scsi;								// Internal data
};


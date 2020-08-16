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
//  [ SCSI device controller ]
//
//---------------------------------------------------------------------------
#pragma once
#include "sasidev_ctrl.h"

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

public:
	// Basic Functions
#ifdef RASCSI
	SCSIDEV();
#else
	SCSIDEV(Device *dev);
#endif // RASCSI
										// Constructor

	void FASTCALL Reset();
										// Device Reset

	// 外部API
	BUS::phase_t FASTCALL Process();
										// Run

	void FASTCALL SyncTransfer(BOOL enable) { scsi.syncenable = enable; }
										// Synchronouse transfer enable setting

	// Other
	BOOL FASTCALL IsSASI() const {return FALSE;}
										// SASI Check
	BOOL FASTCALL IsSCSI() const {return TRUE;}
										// SCSI check

private:
	// Phase
	void FASTCALL BusFree();
										// Bus free phase
	void FASTCALL Selection();
										// Selection phase
	void FASTCALL Execute();
										// Execution phase
	void FASTCALL MsgOut();
										// Message out phase
	void FASTCALL Error();
										// Common erorr handling

	// commands
	void FASTCALL CmdInquiry();
										// INQUIRY command
	void FASTCALL CmdModeSelect();
										// MODE SELECT command
	void FASTCALL CmdModeSense();
										// MODE SENSE command
	void FASTCALL CmdStartStop();
										// START STOP UNIT command
	void FASTCALL CmdSendDiag();
										// SEND DIAGNOSTIC command
	void FASTCALL CmdRemoval();
										// PREVENT/ALLOW MEDIUM REMOVAL command
	void FASTCALL CmdReadCapacity();
										// READ CAPACITY command
	void FASTCALL CmdRead10();
										// READ(10) command
	void FASTCALL CmdWrite10();
										// WRITE(10) command
	void FASTCALL CmdSeek10();
										// SEEK(10) command
	void FASTCALL CmdVerify();
										// VERIFY command
	void FASTCALL CmdSynchronizeCache();
										// SYNCHRONIZE CACHE  command
	void FASTCALL CmdReadDefectData10();
										// READ DEFECT DATA(10)  command
	void FASTCALL CmdReadToc();
										// READ TOC command
	void FASTCALL CmdPlayAudio10();
										// PLAY AUDIO(10) command
	void FASTCALL CmdPlayAudioMSF();
										// PLAY AUDIO MSF command
	void FASTCALL CmdPlayAudioTrack();
										// PLAY AUDIO TRACK INDEX command
	void FASTCALL CmdModeSelect10();
										// MODE SELECT(10) command
	void FASTCALL CmdModeSense10();
										// MODE SENSE(10) command
	void FASTCALL CmdGetMessage10();
										// GET MESSAGE(10) command
	void FASTCALL CmdSendMessage10();
										// SEND MESSAGE(10) command

	// データ転送
	void FASTCALL Send();
										// Send data
#ifndef RASCSI
	void FASTCALL SendNext();
										// Continue sending data
#endif	// RASCSI
	void FASTCALL Receive();
										// Receive data
#ifndef RASCSI
	void FASTCALL ReceiveNext();
										// Continue receiving data
#endif	// RASCSI
	BOOL FASTCALL XferMsg(DWORD msg);
										// Data transfer message

	scsi_t scsi;
										// Internal data
};


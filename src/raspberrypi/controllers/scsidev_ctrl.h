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
#include "controllers/sasidev_ctrl.h"


//===========================================================================
//
//	Nuvolink Register constants
//      Bit 0: set when packet received intact, thus always set.
//      Bit 5: set when the packet was a multicast/broadcast packet.
//===========================================================================
#define NUVOLINK_RSR_REG_PACKET_INTACT (1 << 0)
#define NUVOLINK_RSR_REG_MCAST_OR_BCAST (1 << 5)

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


	enum scsi_command : BYTE {
			eCmdTestUnitReady = 0x00,
			eCmdRezero =  0x01,
			eCmdResetStatistics = 0x02, // Nuvolink specific command
			eCmdRequestSense = 0x03,
			eCmdFormat = 0x04,
			eCmdSendPacket = 0x05, 	    // Nuvolink specific command
			eCmdChangeMacAddr = 0x06,   // Nuvolink specific command
			eCmdReassign = 0x07,
			eCmdRead6 = 0x08,
			eCmdSetMcastReg = 0x09,     // Nuvolink specific command
			eCmdWrite6 = 0x0A,
			eCmdSeek6 = 0x0B,
			eCmdMediaSense = 0x0C,      // Nuvolink specific command
			eCmdInquiry = 0x12,
			eCmdModeSelect = 0x15,
			eCmdModeSense = 0x1A,
			eCmdStartStop = 0x1B,
			eCmdRcvDiag = 0x1C,
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
			eCmdModeSense10 = 0x5A,
			eCmdInvalid = 0xC2,		// (SASI only/Suppress warning when using SxSI)
			eCmdSasiCmdAssign = 0x0e, // This isn't used by SCSI, and can probably be removed.
	};

public:
	// Basic Functions
	SCSIDEV();
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

	BOOL FASTCALL TransferPacketToHost(int packet_len);

private:
	// Phase
	void FASTCALL BusFree();
										// Bus free phase
	void FASTCALL Arbitration();
										// Arbitration phase
	void FASTCALL Selection();
										// Selection phase
	void FASTCALL Reselection();
										// Reselection phase
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
	void FASTCALL CmdSendMessage6();
										// SEND MESSAGE(6) command
	void FASTCALL CmdMediaSense();
										// Nuvolink-specific command
	void FASTCALL CmdResetStatistics();
										// Nuvolink-specific command
	void FASTCALL CmdSendPacket();
										// Nuvolink-specific command
	void FASTCALL CmdChangeMacAddr();
										// Nuvolink-specific command
	void FASTCALL CmdSetMcastReg();
										// Nuvolink-specific command

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

	// Sequence number for the Nuvolink. This really belongs somewhere else,
	// but it goes here for now.
	BYTE m_sequence_number = 0;
};


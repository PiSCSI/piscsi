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
#include "controllers/scsidev_ctrl.h"
#include "gpiobus.h"
#include "devices/scsi_host_bridge.h"

//===========================================================================
//
//	SCSI Device
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
#ifdef RASCSI
SCSIDEV::SCSIDEV() : SASIDEV()
#else
SCSIDEV::SCSIDEV(Device *dev) : SASIDEV(dev)
#endif
{
	// Synchronous transfer work initialization
	scsi.syncenable = FALSE;
	scsi.syncperiod = 50;
	scsi.syncoffset = 0;
	scsi.atnmsg = FALSE;
	scsi.msc = 0;
	memset(scsi.msb, 0x00, sizeof(scsi.msb));
}

//---------------------------------------------------------------------------
//
//	Device reset
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::Reset()
{
	ASSERT(this);

	// Work initialization
	scsi.atnmsg = FALSE;
	scsi.msc = 0;
	memset(scsi.msb, 0x00, sizeof(scsi.msb));

	// Base class
	SASIDEV::Reset();
}

//---------------------------------------------------------------------------
//
//	Process
//
//---------------------------------------------------------------------------
BUS::phase_t FASTCALL SCSIDEV::Process()
{
	ASSERT(this);

	// Do nothing if not connected
	if (ctrl.id < 0 || ctrl.bus == NULL) {
		return ctrl.phase;
	}

	// Get bus information
	ctrl.bus->Aquire();

	// Reset
	if (ctrl.bus->GetRST()) {
#if defined(DISK_LOG)
		Log(Log::Normal, "RESET信号受信");
#endif	// DISK_LOG

		// Reset the controller
		Reset();

		// Reset the bus
		ctrl.bus->Reset();
		return ctrl.phase;
	}

	// Phase processing
	switch (ctrl.phase) {
		// Bus free phase
		case BUS::busfree:
			BusFree();
			break;

		// Selection phase
		case BUS::selection:
			Selection();
			break;

		// Data out (MCI=000)
		case BUS::dataout:
			DataOut();
			break;

		// Data in (MCI=001)
		case BUS::datain:
			DataIn();
			break;

		// Command (MCI=010)
		case BUS::command:
			Command();
			break;

		// Status (MCI=011)
		case BUS::status:
			Status();
			break;

		// Message out (MCI=110)
		case BUS::msgout:
			MsgOut();
			break;

		// Message in (MCI=111)
		case BUS::msgin:
			MsgIn();
			break;

		// Other
		default:
			ASSERT(FALSE);
			break;
	}

	return ctrl.phase;
}

//---------------------------------------------------------------------------
//
//	Phaes
//
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//
//	Bus free phase
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::BusFree()
{
	ASSERT(this);

	// Phase change
	if (ctrl.phase != BUS::busfree) {

#if defined(DISK_LOG)
		Log(Log::Normal, "Bus free phase");
#endif	// DISK_LOG

		// Phase setting
		ctrl.phase = BUS::busfree;

		// Signal line
		ctrl.bus->SetREQ(FALSE);
		ctrl.bus->SetMSG(FALSE);
		ctrl.bus->SetCD(FALSE);
		ctrl.bus->SetIO(FALSE);
		ctrl.bus->SetBSY(FALSE);

		// Initialize status and message
		ctrl.status = 0x00;
		ctrl.message = 0x00;

		// Initialize ATN message reception status
		scsi.atnmsg = FALSE;
		return;
	}

	// Move to selection phase
	if (ctrl.bus->GetSEL() && !ctrl.bus->GetBSY()) {
		Selection();
	}
}

//---------------------------------------------------------------------------
//
//	Selection Phase
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::Selection()
{
	DWORD id;

	ASSERT(this);

	// Phase change
	if (ctrl.phase != BUS::selection) {
		// invalid if IDs do not match
		id = 1 << ctrl.id;
		if ((ctrl.bus->GetDAT() & id) == 0) {
			return;
		}

		// End if there is no valid unit
		if (!HasUnit()) {
			return;
		}

#if defined(DISK_LOG)
		Log(Log::Normal,
			"Selection Phase ID=%d (with device)", ctrl.id);
#endif	// DISK_LOG

		// Phase setting
		ctrl.phase = BUS::selection;

		// Raise BSY and respond
		ctrl.bus->SetBSY(TRUE);
		return;
	}

	// Selection completed
	if (!ctrl.bus->GetSEL() && ctrl.bus->GetBSY()) {
		// Message out phase if ATN=1, otherwise command phase
		if (ctrl.bus->GetATN()) {
			MsgOut();
		} else {
			Command();
		}
	}
}

//---------------------------------------------------------------------------
//
//	Execution Phase
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::Execute()
{
	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "Execution phase command $%02X", ctrl.cmd[0]);
#endif	// DISK_LOG

	// Phase Setting
	ctrl.phase = BUS::execute;

	// Initialization for data transfer
	ctrl.offset = 0;
	ctrl.blocks = 1;
#ifdef RASCSI
	ctrl.execstart = SysTimer::GetTimerLow();
#endif	// RASCSI

	// Process by command
	switch (ctrl.cmd[0]) {
		// TEST UNIT READY
		case 0x00:
			CmdTestUnitReady();
			return;

		// REZERO
		case 0x01:
			CmdRezero();
			return;

		// REQUEST SENSE
		case 0x03:
			CmdRequestSense();
			return;

		// FORMAT UNIT
		case 0x04:
			CmdFormat();
			return;

		// REASSIGN BLOCKS
		case 0x07:
			CmdReassign();
			return;

		// READ(6)
		case 0x08:
			CmdRead6();
			return;

		// WRITE(6)
		case 0x0a:
			CmdWrite6();
			return;

		// SEEK(6)
		case 0x0b:
			CmdSeek6();
			return;

		// INQUIRY
		case 0x12:
			CmdInquiry();
			return;

		// MODE SELECT
		case 0x15:
			CmdModeSelect();
			return;

		// MDOE SENSE
		case 0x1a:
			CmdModeSense();
			return;

		// START STOP UNIT
		case 0x1b:
			CmdStartStop();
			return;

		// SEND DIAGNOSTIC
		case 0x1d:
			CmdSendDiag();
			return;

		// PREVENT/ALLOW MEDIUM REMOVAL
		case 0x1e:
			CmdRemoval();
			return;

		// READ CAPACITY
		case 0x25:
			CmdReadCapacity();
			return;

		// READ(10)
		case 0x28:
			CmdRead10();
			return;

		// WRITE(10)
		case 0x2a:
			CmdWrite10();
			return;

		// SEEK(10)
		case 0x2b:
			CmdSeek10();
			return;

		// WRITE and VERIFY
		case 0x2e:
			CmdWrite10();
			return;

		// VERIFY
		case 0x2f:
			CmdVerify();
			return;

		// SYNCHRONIZE CACHE
		case 0x35:
			CmdSynchronizeCache();
			return;

		// READ DEFECT DATA(10)
		case 0x37:
			CmdReadDefectData10();
			return;

		// READ TOC
		case 0x43:
			CmdReadToc();
			return;

		// PLAY AUDIO(10)
		case 0x45:
			CmdPlayAudio10();
			return;

		// PLAY AUDIO MSF
		case 0x47:
			CmdPlayAudioMSF();
			return;

		// PLAY AUDIO TRACK
		case 0x48:
			CmdPlayAudioTrack();
			return;

		// MODE SELECT(10)
		case 0x55:
			CmdModeSelect10();
			return;

		// MDOE SENSE(10)
		case 0x5a:
			CmdModeSense10();
			return;

		// SPECIFY (SASI only/Suppress warning when using SxSI)
		case 0xc2:
			CmdInvalid();
			return;

	}

	// No other support
	Log(Log::Normal, "Unsupported command received: $%02X", ctrl.cmd[0]);
	CmdInvalid();
}

//---------------------------------------------------------------------------
//
//	Message out phase
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::MsgOut()
{
	ASSERT(this);

	// Phase change
	if (ctrl.phase != BUS::msgout) {

#if defined(DISK_LOG)
		Log(Log::Normal, "Message Out Phase");
#endif	// DISK_LOG

		// Message out phase after selection
        // process the IDENTIFY message
		if (ctrl.phase == BUS::selection) {
			scsi.atnmsg = TRUE;
			scsi.msc = 0;
			memset(scsi.msb, 0x00, sizeof(scsi.msb));
		}

		// Phase Setting
		ctrl.phase = BUS::msgout;

		// Signal line operated by the target
		ctrl.bus->SetMSG(TRUE);
		ctrl.bus->SetCD(TRUE);
		ctrl.bus->SetIO(FALSE);

		// Data transfer is 1 byte x 1 block
		ctrl.offset = 0;
		ctrl.length = 1;
		ctrl.blocks = 1;

#ifndef RASCSI
		// Request message
		ctrl.bus->SetREQ(TRUE);
#endif	// RASCSI
		return;
	}

#ifdef RASCSI
	// Receive
	Receive();
#else
	// Requesting
	if (ctrl.bus->GetREQ()) {
		// Sent by the initiator
		if (ctrl.bus->GetACK()) {
			Receive();
		}
	} else {
		// Request the initator to
		if (!ctrl.bus->GetACK()) {
			ReceiveNext();
		}
	}
#endif	// RASCSI
}

//---------------------------------------------------------------------------
//
//	Common Error Handling
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::Error()
{
	ASSERT(this);

	// Get bus information
	ctrl.bus->Aquire();

	// Reset check
	if (ctrl.bus->GetRST()) {
		// Reset the controller
		Reset();

		// Reset the bus
		ctrl.bus->Reset();
		return;
	}

	// Bus free for status phase and message in phase
	if (ctrl.phase == BUS::status || ctrl.phase == BUS::msgin) {
		BusFree();
		return;
	}

#if defined(DISK_LOG)
	Log(Log::Normal, "Error (to status phase)");
#endif	// DISK_LOG

	// Set status and message(CHECK CONDITION)
	ctrl.status = 0x02;
	ctrl.message = 0x00;

	// status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	Command
//
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//
//	INQUIRY
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdInquiry()
{
	Disk *disk;
	int lun;
	DWORD major;
	DWORD minor;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "INQUIRY Command");
#endif	// DISK_LOG

	// Find a valid unit
	disk = NULL;
	for (lun = 0; lun < UnitMax; lun++) {
		if (ctrl.unit[lun]) {
			disk = ctrl.unit[lun];
			break;
		}
	}

	// Processed on the disk side (it is originally processed by the controller)
	if (disk) {
#ifdef RASCSI
		major = (DWORD)(RASCSI >> 8);
		minor = (DWORD)(RASCSI & 0xff);
#else
		host->GetVM()->GetVersion(major, minor);
#endif	// RASCSI
		ctrl.length =
			ctrl.unit[lun]->Inquiry(ctrl.cmd, ctrl.buffer, major, minor);
	} else {
		ctrl.length = 0;
	}

	if (ctrl.length <= 0) {
		// failure (error)
		Error();
		return;
	}

	// Add synchronous transfer support information
	if (scsi.syncenable) {
		ctrl.buffer[7] |= (1 << 4);
	}

	// Data-in Phase
	DataIn();
}

//---------------------------------------------------------------------------
//
//	MODE SELECT
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdModeSelect()
{
	DWORD lun;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "MODE SELECT Command");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	ctrl.length = ctrl.unit[lun]->SelectCheck(ctrl.cmd);
	if (ctrl.length <= 0) {
		// Failure (Error)
		Error();
		return;
	}

	// Data out phase
	DataOut();
}

//---------------------------------------------------------------------------
//
//	MODE SENSE
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdModeSense()
{
	DWORD lun;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "MODE SENSE Command ");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	ctrl.length = ctrl.unit[lun]->ModeSense(ctrl.cmd, ctrl.buffer);
	ASSERT(ctrl.length >= 0);
	if (ctrl.length == 0) {
		Log(Log::Warning,
			"Not supported MODE SENSE page $%02X", ctrl.cmd[2]);

		// Failure (Error)
		Error();
		return;
	}

	// Data-in Phase
	DataIn();
}

//---------------------------------------------------------------------------
//
//	START STOP UNIT
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdStartStop()
{
	DWORD lun;
	BOOL status;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "START STOP UNIT Command ");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	status = ctrl.unit[lun]->StartStop(ctrl.cmd);
	if (!status) {
		// Failure (Error)
		Error();
		return;
	}

	// status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	SEND DIAGNOSTIC
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdSendDiag()
{
	DWORD lun;
	BOOL status;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "SEND DIAGNOSTIC Command ");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	status = ctrl.unit[lun]->SendDiag(ctrl.cmd);
	if (!status) {
		// Failure (Error)
		Error();
		return;
	}

	// status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	PREVENT/ALLOW MEDIUM REMOVAL
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdRemoval()
{
	DWORD lun;
	BOOL status;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "PREVENT/ALLOW MEDIUM REMOVAL Command ");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	status = ctrl.unit[lun]->Removal(ctrl.cmd);
	if (!status) {
		// Failure (Error)
		Error();
		return;
	}

	// status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	READ CAPACITY
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdReadCapacity()
{
	DWORD lun;
	int length;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "READ CAPACITY Command ");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	length = ctrl.unit[lun]->ReadCapacity(ctrl.cmd, ctrl.buffer);
	ASSERT(length >= 0);
	if (length <= 0) {
		Error();
		return;
	}

	// Length setting
	ctrl.length = length;

	// Data-in Phase
	DataIn();
}

//---------------------------------------------------------------------------
//
//	READ(10)
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdRead10()
{
	DWORD lun;
	DWORD record;

	ASSERT(this);

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Receive message if host bridge
	if (ctrl.unit[lun]->GetID() == MAKEID('S', 'C', 'B', 'R')) {
		CmdGetMessage10();
		return;
	}

	// Get record number and block number
	record = ctrl.cmd[2];
	record <<= 8;
	record |= ctrl.cmd[3];
	record <<= 8;
	record |= ctrl.cmd[4];
	record <<= 8;
	record |= ctrl.cmd[5];
	ctrl.blocks = ctrl.cmd[7];
	ctrl.blocks <<= 8;
	ctrl.blocks |= ctrl.cmd[8];

#if defined(DISK_LOG)
	Log(Log::Normal, "READ(10) command record=%08X block=%d", record, ctrl.blocks);
#endif	// DISK_LOG

	// Do not process 0 blocks
	if (ctrl.blocks == 0) {
		Status();
		return;
	}

	// Command processing on drive
	ctrl.length = ctrl.unit[lun]->Read(ctrl.buffer, record);
	if (ctrl.length <= 0) {
		// Failure (Error)
		Error();
		return;
	}

	// Set next block
	ctrl.next = record + 1;

	// Data-in Phase
	DataIn();
}

//---------------------------------------------------------------------------
//
//	WRITE(10)
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdWrite10()
{
	DWORD lun;
	DWORD record;

	ASSERT(this);

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Receive message with host bridge
	if (ctrl.unit[lun]->GetID() == MAKEID('S', 'C', 'B', 'R')) {
		CmdSendMessage10();
		return;
	}

	// Get record number and block number
	record = ctrl.cmd[2];
	record <<= 8;
	record |= ctrl.cmd[3];
	record <<= 8;
	record |= ctrl.cmd[4];
	record <<= 8;
	record |= ctrl.cmd[5];
	ctrl.blocks = ctrl.cmd[7];
	ctrl.blocks <<= 8;
	ctrl.blocks |= ctrl.cmd[8];

#if defined(DISK_LOG)
	Log(Log::Normal,
		"WRTIE(10) command record=%08X blocks=%d", record, ctrl.blocks);
#endif	// DISK_LOG

	// Do not process 0 blocks
	if (ctrl.blocks == 0) {
		Status();
		return;
	}

	// Command processing on drive
	ctrl.length = ctrl.unit[lun]->WriteCheck(record);
	if (ctrl.length <= 0) {
		// Failure (Error)
		Error();
		return;
	}

	// Set next block
	ctrl.next = record + 1;

	// Data out phase
	DataOut();
}

//---------------------------------------------------------------------------
//
//	SEEK(10)
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdSeek10()
{
	DWORD lun;
	BOOL status;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "SEEK(10) Command ");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	status = ctrl.unit[lun]->Seek(ctrl.cmd);
	if (!status) {
		// Failure (Error)
		Error();
		return;
	}

	// status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	VERIFY
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdVerify()
{
	DWORD lun;
	BOOL status;
	DWORD record;

	ASSERT(this);

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Get record number and block number
	record = ctrl.cmd[2];
	record <<= 8;
	record |= ctrl.cmd[3];
	record <<= 8;
	record |= ctrl.cmd[4];
	record <<= 8;
	record |= ctrl.cmd[5];
	ctrl.blocks = ctrl.cmd[7];
	ctrl.blocks <<= 8;
	ctrl.blocks |= ctrl.cmd[8];

#if defined(DISK_LOG)
	Log(Log::Normal,
		"VERIFY command record=%08X blocks=%d", record, ctrl.blocks);
#endif	// DISK_LOG

	// Do not process 0 blocks
	if (ctrl.blocks == 0) {
		Status();
		return;
	}

	// if BytChk=0
	if ((ctrl.cmd[1] & 0x02) == 0) {
		// Command processing on drive
		status = ctrl.unit[lun]->Seek(ctrl.cmd);
		if (!status) {
			// Failure (Error)
			Error();
			return;
		}

		// status phase
		Status();
		return;
	}

	// Test loading
	ctrl.length = ctrl.unit[lun]->Read(ctrl.buffer, record);
	if (ctrl.length <= 0) {
		// Failure (Error)
		Error();
		return;
	}

	// Set next block
	ctrl.next = record + 1;

	// Data out phase
	DataOut();
}

//---------------------------------------------------------------------------
//
//	SYNCHRONIZE CACHE
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdSynchronizeCache()
{
	DWORD lun;

	ASSERT(this);

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Make it do something (not implemented)...

	// status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	READ DEFECT DATA(10)
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdReadDefectData10()
{
	DWORD lun;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "READ DEFECT DATA(10) Command ");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	ctrl.length = ctrl.unit[lun]->ReadDefectData10(ctrl.cmd, ctrl.buffer);
	ASSERT(ctrl.length >= 0);

	if (ctrl.length <= 4) {
		Error();
		return;
	}

	// Data-in Phase
	DataIn();
}

//---------------------------------------------------------------------------
//
//	READ TOC
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdReadToc()
{
	DWORD lun;

	ASSERT(this);

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	ctrl.length = ctrl.unit[lun]->ReadToc(ctrl.cmd, ctrl.buffer);
	if (ctrl.length <= 0) {
		// Failure (Error)
		Error();
		return;
	}

	// Data-in Phase
	DataIn();
}

//---------------------------------------------------------------------------
//
//	PLAY AUDIO(10)
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdPlayAudio10()
{
	DWORD lun;
	BOOL status;

	ASSERT(this);

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	status = ctrl.unit[lun]->PlayAudio(ctrl.cmd);
	if (!status) {
		// Failure (Error)
		Error();
		return;
	}

	// status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	PLAY AUDIO MSF
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdPlayAudioMSF()
{
	DWORD lun;
	BOOL status;

	ASSERT(this);

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	status = ctrl.unit[lun]->PlayAudioMSF(ctrl.cmd);
	if (!status) {
		// Failure (Error)
		Error();
		return;
	}

	// status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	PLAY AUDIO TRACK
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdPlayAudioTrack()
{
	DWORD lun;
	BOOL status;

	ASSERT(this);

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	status = ctrl.unit[lun]->PlayAudioTrack(ctrl.cmd);
	if (!status) {
		// Failure (Error)
		Error();
		return;
	}

	// status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	MODE SELECT10
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdModeSelect10()
{
	DWORD lun;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "MODE SELECT10 Command ");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	ctrl.length = ctrl.unit[lun]->SelectCheck10(ctrl.cmd);
	if (ctrl.length <= 0) {
		// Failure (Error)
		Error();
		return;
	}

	// Data out phase
	DataOut();
}

//---------------------------------------------------------------------------
//
//	MODE SENSE(10)
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdModeSense10()
{
	DWORD lun;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "MODE SENSE(10) Command ");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	ctrl.length = ctrl.unit[lun]->ModeSense10(ctrl.cmd, ctrl.buffer);
	ASSERT(ctrl.length >= 0);
	if (ctrl.length == 0) {
		Log(Log::Warning,
			"Not supported MODE SENSE(10) page $%02X", ctrl.cmd[2]);

		// Failure (Error)
		Error();
		return;
	}

	// Data-in Phase
	DataIn();
}

//---------------------------------------------------------------------------
//
//	GET MESSAGE(10)
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdGetMessage10()
{
	DWORD lun;
	SCSIBR *bridge;

	ASSERT(this);

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Error if not a host bridge
	if (ctrl.unit[lun]->GetID() != MAKEID('S', 'C', 'B', 'R')) {
		Error();
		return;
	}

	// Reallocate buffer (because it is not transfer for each block)
	if (ctrl.bufsize < 0x1000000) {
		free(ctrl.buffer);
		ctrl.bufsize = 0x1000000;
		ctrl.buffer = (BYTE *)malloc(ctrl.bufsize);
	}

	// Process with drive
	bridge = (SCSIBR*)ctrl.unit[lun];
	ctrl.length = bridge->GetMessage10(ctrl.cmd, ctrl.buffer);

	if (ctrl.length <= 0) {
		// Failure (Error)
		Error();
		return;
	}

	// Set next block
	ctrl.blocks = 1;
	ctrl.next = 1;

	// Data in phase
	DataIn();
}

//---------------------------------------------------------------------------
//
//	SEND MESSAGE(10)
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdSendMessage10()
{
	DWORD lun;

	ASSERT(this);

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Error if not a host bridge
	if (ctrl.unit[lun]->GetID() != MAKEID('S', 'C', 'B', 'R')) {
		Error();
		return;
	}

	// Reallocate buffer (because it is not transfer for each block)
	if (ctrl.bufsize < 0x1000000) {
		free(ctrl.buffer);
		ctrl.bufsize = 0x1000000;
		ctrl.buffer = (BYTE *)malloc(ctrl.bufsize);
	}

	// Set transfer amount
	ctrl.length = ctrl.cmd[6];
	ctrl.length <<= 8;
	ctrl.length |= ctrl.cmd[7];
	ctrl.length <<= 8;
	ctrl.length |= ctrl.cmd[8];

	if (ctrl.length <= 0) {
		// Failure (Error)
		Error();
		return;
	}

	// Set next block
	ctrl.blocks = 1;
	ctrl.next = 1;

	// Light phase
	DataOut();
}

//===========================================================================
//
//	Data Transfer
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Send data
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::Send()
{
#ifdef RASCSI
	int len;
#endif	// RASCSI
	BOOL result;

	ASSERT(this);
	ASSERT(!ctrl.bus->GetREQ());
	ASSERT(ctrl.bus->GetIO());

#ifdef RASCSI
	//if Length! = 0, send
	if (ctrl.length != 0) {
		len = ctrl.bus->SendHandShake(
			&ctrl.buffer[ctrl.offset], ctrl.length);

		// If you cannot send all, move to status phase
		if (len != (int)ctrl.length) {
			Error();
			return;
		}

		// offset and length
		ctrl.offset += ctrl.length;
		ctrl.length = 0;
		return;
	}
#else
	// offset and length
	ASSERT(ctrl.length >= 1);
	ctrl.offset++;
	ctrl.length--;

	// Immediately after ACK is asserted, if the data has been
    // set by SendNext, raise the request
	if (ctrl.length != 0) {
		// Signal line operated by the target
		ctrl.bus->SetREQ(TRUE);
		return;
	}
#endif	// RASCSI

	// Block subtraction, result initialization
	ctrl.blocks--;
	result = TRUE;

	// Processing after data collection (read/data-in only)
	if (ctrl.phase == BUS::datain) {
		if (ctrl.blocks != 0) {
			// // set next buffer (set offset, length)
			result = XferIn(ctrl.buffer);
#ifndef RASCSI
			ctrl.bus->SetDAT(ctrl.buffer[ctrl.offset]);
#endif	// RASCSI
		}
	}

	// If result FALSE, move to status phase
	if (!result) {
		Error();
		return;
	}

	// Continue sending if block !=0
	if (ctrl.blocks != 0){
		ASSERT(ctrl.length > 0);
		ASSERT(ctrl.offset == 0);
#ifndef RASCSI
		// Signal line operated by the target
		ctrl.bus->SetREQ(TRUE);
#endif	// RASCSI
		return;
	}

	// Move to next phase
	switch (ctrl.phase) {
		// Message in phase
		case BUS::msgin:
			// Completed sending response to extended message of IDENTIFY message
			if (scsi.atnmsg) {
				// flag off
				scsi.atnmsg = FALSE;

				// command phase
				Command();
			} else {
				// Bus free phase
				BusFree();
			}
			break;

		// Data-in Phase
		case BUS::datain:
			// status phase
			Status();
			break;

		// status phase
		case BUS::status:
			// Message in phase
			ctrl.length = 1;
			ctrl.blocks = 1;
			ctrl.buffer[0] = (BYTE)ctrl.message;
			MsgIn();
			break;

		// Other (impossible)
		default:
			ASSERT(FALSE);
			break;
	}
}

#ifndef RASCSI
//---------------------------------------------------------------------------
//
//	Continue data transmission.....
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::SendNext()
{
	ASSERT(this);

	// REQ is up
	ASSERT(ctrl.bus->GetREQ());
	ASSERT(ctrl.bus->GetIO());

	// Signal line operated by the target
	ctrl.bus->SetREQ(FALSE);

	// If there is data in the buffer, set it first
	if (ctrl.length > 1) {
		ctrl.bus->SetDAT(ctrl.buffer[ctrl.offset + 1]);
	}
}
#endif	// RASCSI

#ifndef RASCSI
//---------------------------------------------------------------------------
//
//	Receive data
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::Receive()
{
	DWORD data;

	ASSERT(this);

	// Req is up
	ASSERT(ctrl.bus->GetREQ());
	ASSERT(!ctrl.bus->GetIO());

	// Get data
	data = (DWORD)ctrl.bus->GetDAT();

	// Signal line operated by the target
	ctrl.bus->SetREQ(FALSE);

	switch (ctrl.phase) {
		// Command phase
		case BUS::command:
			ctrl.cmd[ctrl.offset] = data;
#if defined(DISK_LOG)
			Log(Log::Normal, "Command phase $%02X", data);
#endif	// DISK_LOG

			// Set the length again with the first data (offset 0)
			if (ctrl.offset == 0) {
				if (ctrl.cmd[0] >= 0x20) {
					// 10バイトCDB
					ctrl.length = 10;
				}
			}
			break;

		// Message out phase
		case BUS::msgout:
			ctrl.message = data;
#if defined(DISK_LOG)
			Log(Log::Normal, "Message out phase $%02X", data);
#endif	// DISK_LOG
			break;

		// Data out phase
		case BUS::dataout:
			ctrl.buffer[ctrl.offset] = (BYTE)data;
			break;

		// Other (impossible)
		default:
			ASSERT(FALSE);
			break;
	}
}
#endif	// RASCSI

#ifdef RASCSI
//---------------------------------------------------------------------------
//
//  Receive Data
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::Receive()
#else
//---------------------------------------------------------------------------
//
//	Continue receiving data
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::ReceiveNext()
#endif	// RASCSI
{
#ifdef RASCSI
	int len;
#endif	// RASCSI
	BOOL result;
	int i;
	BYTE data;

	ASSERT(this);

	// REQ is low
	ASSERT(!ctrl.bus->GetREQ());
	ASSERT(!ctrl.bus->GetIO());

#ifdef RASCSI
	// Length != 0 if received
	if (ctrl.length != 0) {
		// Receive
		len = ctrl.bus->ReceiveHandShake(
			&ctrl.buffer[ctrl.offset], ctrl.length);

		// If not able to receive all, move to status phase
		if (len != (int)ctrl.length) {
			Error();
			return;
		}

		// Offset and Length
		ctrl.offset += ctrl.length;
		ctrl.length = 0;;
		return;
	}
#else
	// Offset and Length
	ASSERT(ctrl.length >= 1);
	ctrl.offset++;
	ctrl.length--;

	// If length!=0, set req again
	if (ctrl.length != 0) {
		// Signal line operated by the target
		ctrl.bus->SetREQ(TRUE);
		return;
	}
#endif	// RASCSI

	// Block subtraction, result initialization
	ctrl.blocks--;
	result = TRUE;

	// Processing after receiving data (by phase)
	switch (ctrl.phase) {

		// Data out phase
		case BUS::dataout:
			if (ctrl.blocks == 0) {
				// End with this buffer
				result = XferOut(FALSE);
			} else {
				// Continue to next buffer (set offset, length)
				result = XferOut(TRUE);
			}
			break;

		// Message out phase
		case BUS::msgout:
			ctrl.message = ctrl.buffer[0];
			if (!XferMsg(ctrl.message)) {
				// Immediately free the bus if message output fails
				BusFree();
				return;
			}

			// Clear message data in preparation for message-in
			ctrl.message = 0x00;
			break;

		default:
			break;
	}

	// If result FALSE, move to status phase
	if (!result) {
		Error();
		return;
	}

	// Continue to receive if block !=0
	if (ctrl.blocks != 0){
		ASSERT(ctrl.length > 0);
		ASSERT(ctrl.offset == 0);
#ifndef RASCSI
		// Signal line operated by the target
		ctrl.bus->SetREQ(TRUE);
#endif	// RASCSI
		return;
	}

	// Move to next phase
	switch (ctrl.phase) {
		// Command phase
		case BUS::command:
#ifdef RASCSI
			// Command data transfer
			len = 6;
			if (ctrl.buffer[0] >= 0x20 && ctrl.buffer[0] <= 0x7D) {
				// 10 byte CDB
				len = 10;
			}
			for (i = 0; i < len; i++) {
				ctrl.cmd[i] = (DWORD)ctrl.buffer[i];
#if defined(DISK_LOG)
				Log(Log::Normal, "Command $%02X", ctrl.cmd[i]);
#endif	// DISK_LOG
			}
#endif	// RASCSI

			// Execution Phase
			Execute();
			break;

		// Message out phase
		case BUS::msgout:
			// Continue message out phase as long as ATN keeps asserting
			if (ctrl.bus->GetATN()) {
				// Data transfer is 1 byte x 1 block
				ctrl.offset = 0;
				ctrl.length = 1;
				ctrl.blocks = 1;
#ifndef RASCSI
				// Request message
				ctrl.bus->SetREQ(TRUE);
#endif	// RASCSI
				return;
			}

			// Parsing messages sent by ATN
			if (scsi.atnmsg) {
				i = 0;
				while (i < scsi.msc) {
					// Message type
					data = scsi.msb[i];

					// ABORT
					if (data == 0x06) {
#if defined(DISK_LOG)
						Log(Log::Normal,
							"Message code ABORT $%02X", data);
#endif	// DISK_LOG
						BusFree();
						return;
					}

					// BUS DEVICE RESET
					if (data == 0x0C) {
#if defined(DISK_LOG)
						Log(Log::Normal,
							"Message code BUS DEVICE RESET $%02X", data);
#endif	// DISK_LOG
						scsi.syncoffset = 0;
						BusFree();
						return;
					}

					// IDENTIFY
					if (data >= 0x80) {
#if defined(DISK_LOG)
						Log(Log::Normal,
							"Message code IDENTIFY $%02X", data);
#endif	// DISK_LOG
					}

					// Extended Message
					if (data == 0x01) {
#if defined(DISK_LOG)
						Log(Log::Normal,
							"Message code EXTENDED MESSAGE $%02X", data);
#endif	// DISK_LOG

						// Check only when synchronous transfer is possible
						if (!scsi.syncenable || scsi.msb[i + 2] != 0x01) {
							ctrl.length = 1;
							ctrl.blocks = 1;
							ctrl.buffer[0] = 0x07;
							MsgIn();
							return;
						}

						// Transfer period factor (limited to 50 x 4 = 200ns)
						scsi.syncperiod = scsi.msb[i + 3];
						if (scsi.syncperiod > 50) {
							scsi.syncoffset = 50;
						}

						// REQ/ACK offset(limited to 16)
						scsi.syncoffset = scsi.msb[i + 4];
						if (scsi.syncoffset > 16) {
							scsi.syncoffset = 16;
						}

						// STDR response message generation
						ctrl.length = 5;
						ctrl.blocks = 1;
						ctrl.buffer[0] = 0x01;
						ctrl.buffer[1] = 0x03;
						ctrl.buffer[2] = 0x01;
						ctrl.buffer[3] = (BYTE)scsi.syncperiod;
						ctrl.buffer[4] = (BYTE)scsi.syncoffset;
						MsgIn();
						return;
					}

					// next
					i++;
				}
			}

			// Initialize ATN message reception status
			scsi.atnmsg = FALSE;

			// Command phase
			Command();
			break;

		// Data out phase
		case BUS::dataout:
			// Flush unit
			FlushUnit();

			// status phase
			Status();
			break;

		// Other (impossible)
		default:
			ASSERT(FALSE);
			break;
	}
}

//---------------------------------------------------------------------------
//
//	Transfer MSG
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSIDEV::XferMsg(DWORD msg)
{
	ASSERT(this);
	ASSERT(ctrl.phase == BUS::msgout);

	// Save message out data
	if (scsi.atnmsg) {
		scsi.msb[scsi.msc] = (BYTE)msg;
		scsi.msc++;
		scsi.msc %= 256;
	}

	return TRUE;
}

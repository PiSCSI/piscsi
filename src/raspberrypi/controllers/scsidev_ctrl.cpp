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
#include "log.h"
#include "controllers/scsidev_ctrl.h"
#include "gpiobus.h"
#include "devices/scsi_host_bridge.h"
#include "devices/scsi_nuvolink.h"

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

	// Check to see if the reset signal was asserted
	if (ctrl.bus->GetRST()) {
		LOGWARN("RESET signal received!");

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
//	Phases
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
//	Arbitration Phase
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::Arbitration()
{
	// TODO: See https://www.staff.uni-mainz.de/tacke/scsi/SCSI2-06.html
	DWORD id;
	DWORD data_lines;

	ASSERT(this);

	// We need to switch the tranceivers to be inputs....
	ctrl.bus->SetBSY(FALSE); // Make sure that we're not asserting the BSY signal
	ctrl.bus->SetSEL(FALSE); // Make sure that we're not asserting the SEL signal

	// If we arent' in the bus-free phase, we can't progress....
	// just return.
	ctrl.bus->Aquire();
	if(ctrl.bus->GetBSY() || ctrl.bus->GetSEL())
	{
		LOGWARN("Unable to start arbitration. BSY:%d SEL:%d",(int)ctrl.bus->GetBSY(), (int)ctrl.bus->GetSEL());
	}
	
	// Phase change
	if (ctrl.phase != BUS::arbitration) {

		ctrl.phase = BUS::arbitration;

		// Assert both the BSY signal and our own SCSI ID
		id = 1 << ctrl.id;
		ctrl.bus->SetDAT(id);
		ctrl.bus->SetBSY(TRUE);

		// Wait for an ARBITRATION DELAY
		SysTimer::SleepNsec(SCSI_DELAY_ARBITRATION_DELAY_NS);
	
		// Check if a higher SCSI ID is asserted. If so, we lost arbitration
		ctrl.bus->Aquire();
		data_lines = ctrl.bus->GetDAT();
		LOGDEBUG("After Arbitration, data lines are %04X", (int)data_lines);
		data_lines >>= (ctrl.id + 1);
		if(data_lines != 0)
		{
			LOGINFO("We LOST arbitration for ID %d", ctrl.id);
			BusFree();
			return;
		}

		// If we won the arbitration, assert the SEL signal
		ctrl.bus->SetSEL(TRUE);
		
		// Wait for BUS CLEAR delay + BUS SETTLE delay before changing any signals
		SysTimer::SleepNsec(SCSI_DELAY_BUS_CLEAR_DELAY_NS + SCSI_DELAY_BUS_SETTLE_DELAY_NS);
	}
	return;
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
//	Reselection Phase
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::Reselection()
{
	DWORD id;

	ASSERT(this);

	// Phase change
	if (ctrl.phase != BUS::reselection) {
		ctrl.phase = BUS::reselection;

		// Assert the IO signal
		ctrl.bus->SetIO(TRUE);

		// Set the data bus to my SCSI ID or-ed with the Initiator's SCSI ID
		// Assume this is 7, since that is what all Macintoshes use
		id = (1 << ctrl.id) | (1 << 7);
		LOGDEBUG("Reslection DAT set to %02X",(int)id);
		ctrl.bus->SetDAT((BYTE)id);

		// Wait at least two deskew delays
		SysTimer::SleepNsec(SCSI_DELAY_DESKEW_DELAY_NS);
		// Release the BSY signal
		////////////////ctrl.bus->SetBSY(FALSE);
		// We can't use the SetBSY() funciton, because that also reverses the direction of IC3
		ctrl.bus->SetSignal(PIN_BSY, FALSE);

		// Initiater waits for (SEL && IO && ~BSY) with its DAT flag set
		// to accept a reselect
		SysTimer::SleepNsec(SCSI_DELAY_BUS_SETTLE_DELAY_NS);

		// Normally, we should wait to ensure that the target asserts BSY, but we
		// can't read the BSY signal while the IO line is being asserted. So, we just
		// have to assume it worked

		// if(ctrl.bus->WaitSignalTimeoutUs(PIN_BSY, TRUE, SCSI_DELAY_SELECTION_ABORT_TIME_US*3))
		// {
			LOGDEBUG("Initiator correctly asserted BSY");
			// After the Initiator asserts BSY, we need to take it over and also assert it
			////////////////////ctrl.bus->SetBSY(TRUE);
			ctrl.bus->SetSignal(PIN_BSY, TRUE);
			SysTimer::SleepNsec(SCSI_DELAY_DESKEW_DELAY_NS * 2);

			// Release the SEL signal
			ctrl.bus->SetSEL(FALSE);

			// Transition to the Msg Out phase
			MsgOut();
		// }
		// else
		// {
		// 	ctrl.phase = BUS::busfree;
		// 	BusFree();
		// 	// Timeout waiting for Intiaitor to reselect
		// 	LOGERROR("Initiator did not assert PIN_BSY within the specified timeout. Aborting the Reselection");
		// 	// Reset the controller
		// 	Reset();

		// 	// Reset the bus
		// 	ctrl.bus->Reset();
		// 	return;
		// }
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
	switch ((scsi_command)ctrl.cmd[0]) {
		// TEST UNIT READY
		case eCmdTestUnitReady:
			CmdTestUnitReady();
			return;

		// REZERO
		case eCmdRezero:
			CmdRezero();
			return;

		// Nuvolink reset statistics
		case eCmdResetStatistics:
			CmdResetStatistics();
			return;

		// REQUEST SENSE
		case eCmdRequestSense:
			CmdRequestSense();
			return;

		// FORMAT UNIT
		case eCmdFormat:
			CmdFormat();
			return;

		// Nuvolink Send Packet command
		case eCmdSendPacket:
			CmdSendPacket();
			return;

		// Nuvolink Change MAC address command
		case eCmdChangeMacAddr:
			CmdChangeMacAddr();
			return;

		// REASSIGN BLOCKS
		case eCmdReassign:
			CmdReassign();
			return;

		// READ(6)
		case eCmdRead6:
			CmdRead6();
			return;

		case eCmdSetMcastReg:
			CmdSetMcastReg();
			return;

		// WRITE(6)
		case eCmdWrite6:
			CmdWrite6();
			return;

		// SEEK(6)
		case eCmdSeek6:
			CmdSeek6();
			return;

		// Nuvolink Media Sense Command
		case eCmdMediaSense:
			CmdMediaSense();
			return;

		// INQUIRY
		case eCmdInquiry:
			CmdInquiry();
			return;

		// MODE SELECT
		case eCmdModeSelect:
			CmdModeSelect();
			return;

		// MDOE SENSE
		case eCmdModeSense:
			CmdModeSense();
			return;

		// START STOP UNIT
		case eCmdStartStop:
			CmdStartStop();
			return;

		// SEND DIAGNOSTIC
		case eCmdSendDiag:
			CmdSendDiag();
			return;

		// PREVENT/ALLOW MEDIUM REMOVAL
		case eCmdRemoval:
			CmdRemoval();
			return;

		// READ CAPACITY
		case eCmdReadCapacity:
			CmdReadCapacity();
			return;

		// READ(10)
		case eCmdRead10:
			CmdRead10();
			return;

		// WRITE(10)
		case eCmdWrite10:
		// WRITE and VERIFY(10)
		case eCmdWriteAndVerify10:
			CmdWrite10();
			return;

		// SEEK(10)
		case eCmdSeek10:
			CmdSeek10();
			return;

		// VERIFY
		case eCmdVerify:
			CmdVerify();
			return;

		// SYNCHRONIZE CACHE
		case eCmdSynchronizeCache:
			CmdSynchronizeCache();
			return;

		// READ DEFECT DATA(10)
		case eCmdReadDefectData10:
			CmdReadDefectData10();
			return;

		// READ TOC
		case eCmdReadToc:
			CmdReadToc();
			return;

		// PLAY AUDIO(10)
		case eCmdPlayAudio10:
			CmdPlayAudio10();
			return;

		// PLAY AUDIO MSF
		case eCmdPlayAudioMSF:
			CmdPlayAudioMSF();
			return;

		// PLAY AUDIO TRACK
		case eCmdPlayAudioTrack:
			CmdPlayAudioTrack();
			return;

		// MODE SELECT(10)
		case eCmdModeSelect10:
			CmdModeSelect10();
			return;

		// MDOE SENSE(10)
		case eCmdModeSense10:
			CmdModeSense10();
			return;

		// SPECIFY (SASI only/Suppress warning when using SxSI)
		case eCmdInvalid:
			CmdInvalid();
			return;

		default:
			// No other support
			Log(Log::Normal, "Unsupported command received: $%02X", ctrl.cmd[0]);
			CmdInvalid();
	}
	return;

}

//---------------------------------------------------------------------------
//
//	Message out phase
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::MsgOut()
{
	ASSERT(this);
	LOGTRACE("%s ID: %d",__PRETTY_FUNCTION__, this->GetID());

	// Phase change
	if (ctrl.phase != BUS::msgout) {

		LOGTRACE("Message Out Phase");

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

	// Receive
	Receive();
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
	LOGINFO("Buffer size is %d",ctrl.bufsize);
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

//	// Receive message with Nuvolink
//	if (ctrl.unit[lun]->GetID() == MAKEID('S', 'C', 'N', 'L')) {
//		CmdSendMessage10();
//		return;
//	}
// TODO: .... I don't think this is needed (akuker)

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
//	Unknown Vendor-specific command (probably cmmd_mdsens - Medium Sense)
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdMediaSense(){
	ASSERT(this);
	LOGTRACE("<%d> Received CmdMediaSense command for Nuvolink (%s)", this->GetID(), __PRETTY_FUNCTION__);

	Status();
}


void FASTCALL SCSIDEV::CmdResetStatistics(){
	// DWORD lun;

	ASSERT(this);

	LOGTRACE("<%d> Received CmdResetStatistics command for Nuvolink (%s)", this->GetID(), __PRETTY_FUNCTION__);
	
	Status();
}

void FASTCALL SCSIDEV::CmdSendPacket(){

	DWORD lun;
	ASSERT(this);
	LOGTRACE("<%d> Received CmdSendPacket command for Nuvolink (%s)", this->GetID(), __PRETTY_FUNCTION__);

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		LOGERROR("%s Invalid logical unit specified: %d", __PRETTY_FUNCTION__, (WORD)lun);
		Error();
		return;
	}

	// Send Packet with Nuvolink
	if (ctrl.unit[lun]->GetID() == MAKEID('S', 'C', 'N', 'L')) {
		LOGTRACE("%s Moving to CmdSendMessage6()", __PRETTY_FUNCTION__);
		CmdSendMessage6();
		return;
	}
	else
	{
		LOGERROR("Received a CmdSendPacket command for a non-nuvolink device");
	}



	Status();
}
void FASTCALL SCSIDEV::CmdChangeMacAddr(){
	DWORD lun;

	ASSERT(this);
	LOGTRACE("<%d> Received CmdChangeMacAddr command for Nuvolink", this->GetID());

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		LOGERROR("%s Invalid logical unit specified: %d", __PRETTY_FUNCTION__, (WORD)lun);
		Error();
		return;
	}

	// Receive message with Nuvolink
	if (ctrl.unit[lun]->GetID() == MAKEID('S', 'C', 'N', 'L')) {
		LOGTRACE("%s Moving to CmdSendMessage6()", __PRETTY_FUNCTION__);
		CmdSendMessage6();
		return;
	}
	else
	{
		LOGERROR("Received a CmdChangeMacAddr command for a non-nuvolink device");
	}
	Status();
}

void FASTCALL SCSIDEV::CmdSetMcastReg(){
	DWORD lun;

	ASSERT(this);

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		LOGERROR("%s Invalid logical unit specified: %d", __PRETTY_FUNCTION__, (WORD)lun);
		Error();
		return;
	}

	LOGTRACE("<%d> Received CmdSetMcastReg command for Nuvolink", this->GetID());
	// Receive message with Nuvolink
	if (ctrl.unit[lun]->GetID() == MAKEID('S', 'C', 'N', 'L')) {
		LOGTRACE("%s Moving to CmdSendMessage10()", __PRETTY_FUNCTION__);
		CmdSendMessage6();
		return;
	}
	else
	{
		LOGERROR("Received a CmdSetMcastReg command for a non-nuvolink device");
	}
	Status();
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
	if ((ctrl.unit[lun]->GetID() != MAKEID('S', 'C', 'B', 'R')) &&
	    (ctrl.unit[lun]->GetID() != MAKEID('S', 'C', 'N', 'L'))){
		LOGWARN("Received a GetMessage10 command for a non-bridge unit");
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
//  This Send Message command is used by the X68000 host driver
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
		LOGERROR("Received CmdSendMessage10 for a non-bridge device");
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


//---------------------------------------------------------------------------
//
//	SEND MESSAGE(6)
//
//  This Send Message command is used by the Nuvolink
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdSendMessage6()
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
	if (ctrl.unit[lun]->GetID() != MAKEID('S', 'C', 'N', 'L')) {
		LOGERROR("Received CmdSendMessage6 for a non-nuvolink device");
		Error();
		return;
	}

	// Reallocate buffer (because it is not transfer for each block)
	if (ctrl.bufsize < 0x1000000) {
		LOGTRACE("%s Re-allocating ctrl buffer", __PRETTY_FUNCTION__);
		free(ctrl.buffer);
		ctrl.bufsize = 0x1000000;
		ctrl.buffer = (BYTE *)malloc(ctrl.bufsize);
	}

	// Set transfer amount
	ctrl.length = ctrl.cmd[3];
	ctrl.length <<= 8;
	ctrl.length += ctrl.cmd[4];

	for(int i=0; i< 8; i++)
	{
		LOGDEBUG("ctrl.cmd Byte %d: %02X",i, (int)ctrl.cmd[i]);
	}

	LOGTRACE("%s transfer %d bytes",__PRETTY_FUNCTION__, (unsigned int)ctrl.length);

	if (ctrl.length <= 0) {
		// Failure (Error)
		Error();
		return;
	}

	// Set next block
	ctrl.blocks = 1;
	ctrl.next = 1;

	LOGTRACE("%s transitioning to DataOut()",__PRETTY_FUNCTION__);
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

//---------------------------------------------------------------------------
//
//  Receive Data
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::Receive()
{
	int len;
	BOOL result;
	int i;
	BYTE data;

	ASSERT(this);

	LOGTRACE("%s",__PRETTY_FUNCTION__);

	// REQ is low
	ASSERT(!ctrl.bus->GetREQ());
	ASSERT(!ctrl.bus->GetIO());

	// Length != 0 if received
	if (ctrl.length != 0) {
		LOGTRACE("%s length was != 0", __PRETTY_FUNCTION__);
		// Receive
		len = ctrl.bus->ReceiveHandShake(
			&ctrl.buffer[ctrl.offset], ctrl.length);

		// If not able to receive all, move to status phase
		if (len != (int)ctrl.length) {
			LOGERROR("%s Not able to receive all data. Going to error",__PRETTY_FUNCTION__);
			Error();
			return;
		}

		// Offset and Length
		ctrl.offset += ctrl.length;
		ctrl.length = 0;
		return;
	}

	// Block subtraction, result initialization
	ctrl.blocks--;
	result = TRUE;

	// Processing after receiving data (by phase)
	LOGTRACE("ctrl.phase: %d",(int)ctrl.phase);
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
		return;
	}

	// Move to next phase
	switch (ctrl.phase) {
		// Command phase
		case BUS::command:
			// Command data transfer
			len = 6;
			if (ctrl.buffer[0] >= 0x20 && ctrl.buffer[0] <= 0x7D) {
				// 10 byte CDB
				len = 10;
			}
			for (i = 0; i < len; i++) {
				ctrl.cmd[i] = (DWORD)ctrl.buffer[i];
				LOGTRACE("%s Command $%02X",__PRETTY_FUNCTION__, (int)ctrl.cmd[i]);
			}

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
						LOGTRACE("Message code ABORT $%02X", (int)data);
						BusFree();
						return;
					}

					// BUS DEVICE RESET
					if (data == 0x0C) {
						LOGTRACE("Message code BUS DEVICE RESET $%02X", (int)data);
						scsi.syncoffset = 0;
						BusFree();
						return;
					}

					// IDENTIFY
					if (data >= 0x80) {
						LOGTRACE("Message code IDENTIFY $%02X", (int)data);
					}

					// Extended Message
					if (data == 0x01) {
						LOGTRACE("Message code EXTENDED MESSAGE $%02X", (int)data);

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

//---------------------------------------------------------------------------
//
//	Transfer IP packet to the host via SCSI
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSIDEV::TransferPacketToHost(int packet_len){

	SCSINuvolink *nuvolink;

	LOGTRACE("%s", __PRETTY_FUNCTION__);
	//*****************************
	// BUS FREE PHASE
	//*****************************
	// We should already be in bus free phase when we enter this function
	BusFree();

	//*****************************
	// ARBITRATION PHASE
	//*****************************
	Arbitration();


	//*****************************
	// Reselection
	//*****************************
	Reselection();

	//*****************************
	// Message OUT (expect a "NO OPERATION")
	//    IF we get a DISCONNECT message, abort sending the message
	//        Transition to MESSAGE IN and send a DISCONNECT
	//        then go to BUS FREE
	//*****************************
	MsgOut();

	LOGTRACE("%s Done with MsgOut", __PRETTY_FUNCTION__);
	//*****************************
	// DATA IN
	//   ... send the packet
	//*****************************
	// The Nuvolink should always be unit 0. Unit 1 is only applicable
	// to SASI devices
	if(ctrl.unit[0]->GetID() == MAKEID('S','C','N','L')){
		ctrl.length = packet_len;
		ctrl.buffer[0] = NUVOLINK_RSR_REG_PACKET_INTACT;
		ctrl.buffer[1] = m_sequence_number++;
		ctrl.buffer[2] = (packet_len & 0xFF);
		ctrl.buffer[3] = (packet_len >> 8) & 0xFF;
		nuvolink = (SCSINuvolink*)ctrl.unit[0];
		memcpy(&(ctrl.buffer[4]), nuvolink->packet_buf, packet_len);
	}else{
		ctrl.buffer[2] = 0;
		ctrl.buffer[3] = 0;
	}

	DataIn();

	//*****************************
	// MESSAGE OUT (expect a "NO OPERATION")
	//*****************************
	MsgOut();

	//*****************************
	// If more packets, go back to DATA IN
	//*****************************

	//*****************************
	// Else
	// MESSAGE IN (sends DISCONNECT)
	//*****************************
	ctrl.blocks = 1;
	ctrl.length = sizeof(scsi_message_code);
	ctrl.buffer[0] = eMsgCodeDisconnect;
	ctrl.message = (DWORD)eMsgCodeDisconnect; // (This is probably redundant and unnecessary?)
	MsgIn();

	//*****************************
	// BUS FREE
	//*****************************
	BusFree();

	return TRUE;
}
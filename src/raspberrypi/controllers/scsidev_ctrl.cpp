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
//  	[ SCSI device controller ]
//
//---------------------------------------------------------------------------
#include "log.h"
#include "controllers/scsidev_ctrl.h"
#include "gpiobus.h"
#include "devices/scsi_host_bridge.h"
#include "devices/scsi_daynaport.h"
#include "exceptions.h"

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

	SetupCommand(0x00, "CmdTestUnitReady", &SCSIDEV::CmdTestUnitReady);
	SetupCommand(0x01, "CmdRezero", &SCSIDEV::CmdRezero);
	SetupCommand(0x03, "CmdRequestSense", &SCSIDEV::CmdRequestSense);
	SetupCommand(0x04, "CmdFormat", &SCSIDEV::CmdFormat);
	SetupCommand(0x07, "CmdReassign", &SCSIDEV::CmdReassign);
	SetupCommand(0x08, "CmdRead6", &SCSIDEV::CmdRead6);
	SetupCommand(0x0a, "CmdWrite6", &SCSIDEV::CmdWrite6);
	SetupCommand(0x0b, "CmdSeek6", &SCSIDEV::CmdSeek6);
	SetupCommand(0x12, "CmdInquiry", &SCSIDEV::CmdInquiry);
	SetupCommand(0x15, "CmdModeSelect", &SCSIDEV::CmdModeSelect);
	SetupCommand(0x16, "CmdReserve6", &SCSIDEV::CmdReserve6);
	SetupCommand(0x17, "CmdRelease6", &SCSIDEV::CmdRelease6);
	SetupCommand(0x1a, "CmdModeSense", &SCSIDEV::CmdModeSense);
	SetupCommand(0x1b, "CmdStartStop", &SCSIDEV::CmdStartStop);
	SetupCommand(0x1d, "CmdSendDiag", &SCSIDEV::CmdSendDiag);
	SetupCommand(0x1e, "CmdRemoval", &SCSIDEV::CmdRemoval);
	SetupCommand(0x25, "CmdReadCapacity", &SCSIDEV::CmdReadCapacity);
	SetupCommand(0x28, "CmdRead10", &SCSIDEV::CmdRead10);
	SetupCommand(0x2a, "CmdWrite10", &SCSIDEV::CmdWrite10);
	SetupCommand(0x2e, "CmdWriteAndVerify10", &SCSIDEV::CmdWrite10);
	SetupCommand(0x2b, "CmdSeek10", &SCSIDEV::CmdSeek10);
	SetupCommand(0x2f, "CmdVerify", &SCSIDEV::CmdVerify);
	SetupCommand(0x35, "CmdSynchronizeCache", &SCSIDEV::CmdSynchronizeCache);
	SetupCommand(0x37, "CmdReadDefectData10", &SCSIDEV::CmdReadDefectData10);
	SetupCommand(0x55, "CmdModeSelect10", &SCSIDEV::CmdModeSelect10);
	SetupCommand(0x56, "CmdReserve10", &SCSIDEV::CmdReserve10);
	SetupCommand(0x57, "CmdRelease10", &SCSIDEV::CmdRelease10);
	SetupCommand(0x5a, "CmdModeSense10", &SCSIDEV::CmdModeSense10);

	// MMC specific. TODO Move to separate class
	SetupCommand(0x43, "CmdReadToc", &SCSIDEV::CmdReadToc);
	SetupCommand(0x45, "CmdPlayAudio10", &SCSIDEV::CmdPlayAudio10);
	SetupCommand(0x47, "CmdPlayAudioMSF", &SCSIDEV::CmdPlayAudioMSF);
	SetupCommand(0x48, "CmdPlayAudioTrack", &SCSIDEV::CmdPlayAudioTrack);

	// DaynaPort specific. TODO Move to separate class
	SetupCommand(0x09, "CmdRetrieveStats", &SCSIDEV::CmdRetrieveStats);
	SetupCommand(0x0c, "CmdSetIfaceMode", &SCSIDEV::CmdSetIfaceMode);
	SetupCommand(0x0d, "CmdSetMcastAddr", &SCSIDEV::CmdSetMcastAddr);
	SetupCommand(0x0e, "CmdEnableInterface", &SCSIDEV::CmdEnableInterface);
}

SCSIDEV::~SCSIDEV()
{
	for (auto const& command : scsi_commands) {
		free(command.second);
	}
}

void FASTCALL SCSIDEV::SetupCommand(BYTE opcode, const char* name, void FASTCALL (SCSIDEV::*execute)(void))
{
	scsi_commands[opcode] = new command_t(name, execute);
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
	if (ctrl.m_scsi_id < 0 || ctrl.bus == NULL) {
		return ctrl.phase;
	}

	// Get bus information
	((GPIOBUS*)ctrl.bus)->Aquire();

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

		LOGTRACE( "%s Bus free phase", __PRETTY_FUNCTION__);

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
		id = 1 << ctrl.m_scsi_id;
		if ((ctrl.bus->GetDAT() & id) == 0) {
			return;
		}

		// End if there is no valid unit
		if (!HasUnit()) {
			return;
		}

		LOGTRACE("%s Selection Phase ID=%d (with device)", __PRETTY_FUNCTION__, (int)ctrl.m_scsi_id);

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

	LOGTRACE( "%s Execution phase command $%02X", __PRETTY_FUNCTION__, (unsigned int)ctrl.cmd[0]);

	// Phase Setting
	ctrl.phase = BUS::execute;

	// Initialization for data transfer
	ctrl.offset = 0;
	ctrl.blocks = 1;
	#ifdef RASCSI
	ctrl.execstart = SysTimer::GetTimerLow();
	#endif	// RASCSI

	// If the command is valid it must be contained in the command map
	if (!scsi_commands.count(ctrl.cmd[0])) {
		LOGWARN("%s Received unsupported command: $%02X", __PRETTY_FUNCTION__, (BYTE)ctrl.cmd[0]);
		CmdInvalid();
		return;
	}

	command_t* command = scsi_commands[ctrl.cmd[0]];

	LOGDEBUG("++++ CMD ++++ %s Received %s ($%02X)", __PRETTY_FUNCTION__, command->name, (unsigned int)ctrl.cmd[0]);

	// Discard pending sense data from the previous command if the current command is not REQUEST SENSE
	if ((unsigned int)ctrl.cmd[0] != 0x03) {
		ctrl.sense_key = 0;
		ctrl.asc = 0;
	}

	// Process by command
	(this->*command->execute)();
}

//---------------------------------------------------------------------------
//
//	Message out phase
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::MsgOut()
{
	ASSERT(this);
	LOGTRACE("%s ID: %d",__PRETTY_FUNCTION__, this->GetSCSIID());

	// Phase change
	if (ctrl.phase != BUS::msgout) {

		LOGTRACE("Message Out Phase");

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
void FASTCALL SCSIDEV::Error(int sense_key, int asc)
{
	ASSERT(this);

	// Get bus information
	((GPIOBUS*)ctrl.bus)->Aquire();

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

	LOGTRACE("%s Sense Key and ASC for subsequent REQUEST SENSE: $%02X, $%02X", __PRETTY_FUNCTION__, sense_key, asc);

	// Set Sense Key and ASC for a subsequent REQUEST SENSE
	ctrl.sense_key = sense_key;
	ctrl.asc = asc;

	// Set status and message(CHECK CONDITION)
	ctrl.status = 0x02;
	ctrl.message = 0x00;

	LOGTRACE("%s Error (to status phase)", __PRETTY_FUNCTION__);

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
	int valid_lun;
	DWORD major;
	DWORD minor;

	ASSERT(this);

	LOGTRACE("%s INQUIRY Command", __PRETTY_FUNCTION__);

	// Find a valid unit
	disk = NULL;
	for (valid_lun = 0; valid_lun < UnitMax; valid_lun++) {
		if (ctrl.unit[valid_lun]) {
			disk = ctrl.unit[valid_lun];
			break;
		}
	}

	// Processed on the disk side (it is originally processed by the controller)
	if (disk) {
		major = (DWORD)(RASCSI >> 8);
		minor = (DWORD)(RASCSI & 0xff);
		LOGTRACE("%s Buffer size is %d",__PRETTY_FUNCTION__, ctrl.bufsize);
		ctrl.length =
			ctrl.unit[valid_lun]->Inquiry(ctrl.cmd, ctrl.buffer, major, minor);
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

	// Report if the device does not support the requested LUN
	DWORD lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		ctrl.buffer[0] |= 0x7f;
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
	ASSERT(this);

	LOGTRACE( "%s MODE SELECT Command", __PRETTY_FUNCTION__);

	DWORD lun = GetLun();

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
//	RESERVE(6)
//
//  The reserve/release commands are only used in multi-initiator
//  environments. RaSCSI doesn't support this use case. However, some old
//  versions of Solaris will issue the reserve/release commands. We will
//  just respond with an OK status.
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdReserve6()
{
	ASSERT(this);
	
	LOGTRACE( "%s Reserve(6) Command", __PRETTY_FUNCTION__);

	// status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	RESERVE(10)
//
//  The reserve/release commands are only used in multi-initiator
//  environments. RaSCSI doesn't support this use case. However, some old
//  versions of Solaris will issue the reserve/release commands. We will
//  just respond with an OK status.
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdReserve10()
{
	ASSERT(this);
	LOGTRACE( "%s Reserve(10) Command", __PRETTY_FUNCTION__);

	// status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	RELEASE(6)
//
//  The reserve/release commands are only used in multi-initiator
//  environments. RaSCSI doesn't support this use case. However, some old
//  versions of Solaris will issue the reserve/release commands. We will
//  just respond with an OK status.
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdRelease6()
{
	ASSERT(this);
	LOGTRACE( "%s Release(6) Command", __PRETTY_FUNCTION__);

	// status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	RELEASE(10)
//
//  The reserve/release commands are only used in multi-initiator
//  environments. RaSCSI doesn't support this use case. However, some old
//  versions of Solaris will issue the reserve/release commands. We will
//  just respond with an OK status.
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdRelease10()
{
	ASSERT(this);
	LOGTRACE( "%s Release(10) Command", __PRETTY_FUNCTION__);

	// status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	MODE SENSE
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdModeSense()
{
	ASSERT(this);

	LOGTRACE( "%s MODE SENSE Command ", __PRETTY_FUNCTION__);

	DWORD lun = GetLun();

	// Command processing on drive
	ctrl.length = ctrl.unit[lun]->ModeSense(ctrl.cmd, ctrl.buffer);
	ASSERT(ctrl.length >= 0);
	if (ctrl.length == 0) {
		LOGWARN("%s Not supported MODE SENSE page $%02X",__PRETTY_FUNCTION__, (unsigned int)ctrl.cmd[2]);

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
	BOOL status;

	ASSERT(this);

	LOGTRACE( "%s START STOP UNIT Command ", __PRETTY_FUNCTION__);

	DWORD lun = GetLun();

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
	BOOL status;

	ASSERT(this);

	LOGTRACE( "%s SEND DIAGNOSTIC Command ", __PRETTY_FUNCTION__);

	DWORD lun = GetLun();

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
	BOOL status;

	ASSERT(this);

	LOGTRACE( "%s PREVENT/ALLOW MEDIUM REMOVAL Command ", __PRETTY_FUNCTION__);

	DWORD lun = GetLun();

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
	int length;

	ASSERT(this);

	LOGTRACE( "%s READ CAPACITY Command ", __PRETTY_FUNCTION__);

	DWORD lun = GetLun();

	// Command processing on drive
	length = ctrl.unit[lun]->ReadCapacity(ctrl.cmd, ctrl.buffer);
	if (length <= 0) {
		// MEDIUM NOT PRESENT
		Error(0x05, 0x3a);
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
	DWORD record;

	ASSERT(this);

	DWORD lun = GetLun();

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

	LOGTRACE("%s READ(10) command record=%08X block=%d", __PRETTY_FUNCTION__, (unsigned int)record, (int)ctrl.blocks);

	// Do not process 0 blocks
	if (ctrl.blocks == 0) {
		Status();
		return;
	}

	// Command processing on drive
	ctrl.length = ctrl.unit[lun]->Read(ctrl.cmd, ctrl.buffer, record);
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
	DWORD record;

	ASSERT(this);

	DWORD lun = GetLun();

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

	LOGTRACE("%s WRTIE(10) command record=%08X blocks=%d",__PRETTY_FUNCTION__, (unsigned int)record, (unsigned int)ctrl.blocks);

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
	BOOL status;

	ASSERT(this);

	LOGTRACE( "%s SEEK(10) Command ", __PRETTY_FUNCTION__);

	DWORD lun = GetLun();

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
	BOOL status;
	DWORD record;

	ASSERT(this);

	DWORD lun = GetLun();

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

	LOGTRACE("%s VERIFY command record=%08X blocks=%d",__PRETTY_FUNCTION__, (unsigned int)record, (int)ctrl.blocks);

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
	ctrl.length = ctrl.unit[lun]->Read(ctrl.cmd, ctrl.buffer, record);
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
	ASSERT(this);

	GetLun();

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
	ASSERT(this);

	LOGTRACE( "%s READ DEFECT DATA(10) Command ", __PRETTY_FUNCTION__);

	DWORD lun = GetLun();

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
	ASSERT(this);

	DWORD lun = GetLun();

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
	BOOL status;

	ASSERT(this);

	DWORD lun = GetLun();

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
	BOOL status;

	ASSERT(this);

	DWORD lun = GetLun();

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
	BOOL status;

	ASSERT(this);

	DWORD lun = GetLun();

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
	ASSERT(this);

	LOGTRACE( "%s MODE SELECT10 Command ", __PRETTY_FUNCTION__);

	DWORD lun = GetLun();

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
	ASSERT(this);

	LOGTRACE( "%s MODE SENSE(10) Command ", __PRETTY_FUNCTION__);

	DWORD lun = GetLun();

	// Command processing on drive
	ctrl.length = ctrl.unit[lun]->ModeSense10(ctrl.cmd, ctrl.buffer);
	ASSERT(ctrl.length >= 0);
	if (ctrl.length == 0) {
		LOGWARN("%s Not supported MODE SENSE(10) page $%02X", __PRETTY_FUNCTION__, (WORD)ctrl.cmd[2]);

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
	SCSIBR *bridge;

	ASSERT(this);

	DWORD lun = GetLun();

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
	ASSERT(this);

	DWORD lun = GetLun();

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
// Retrieve Statistics (09)
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdRetrieveStats()
{
	SCSIDaynaPort *dayna_port;

	ASSERT(this);

	DWORD lun = GetLun();

	// Error if not a DaynaPort SCSI Link
	if (ctrl.unit[lun]->GetID() != MAKEID('S', 'C', 'D', 'P')){
		LOGWARN("Received a CmdRetrieveStats command for a non-daynaport unit %08X", (unsigned int)ctrl.unit[lun]->GetID());
		Error();
		return;
	}

	// Process with drive
	dayna_port = (SCSIDaynaPort*)ctrl.unit[lun];
	ctrl.length = dayna_port->RetrieveStats(ctrl.cmd, ctrl.buffer);

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
// Set Interface Mode (0c)
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdSetIfaceMode()
{
	SCSIDaynaPort *dayna_port;

	ASSERT(this);

	LOGTRACE("%s",__PRETTY_FUNCTION__);

	DWORD lun = GetLun();

	// Error if not a DaynaPort SCSI Link
	if (ctrl.unit[lun]->GetID() != MAKEID('S', 'C', 'D', 'P')){
		LOGWARN("%s Received a CmdRetrieveStats command for a non-daynaport unit %08X", __PRETTY_FUNCTION__, (unsigned int)ctrl.unit[lun]->GetID());
		Error();
		return;
	}

	dayna_port = (SCSIDaynaPort*)ctrl.unit[lun];

	// Check whether this command is telling us to "Set Interface Mode" 
	// or "Set MAC Address"

	ctrl.length = dayna_port->RetrieveStats(ctrl.cmd, ctrl.buffer);
	switch(ctrl.cmd[5]){
		case SCSIDaynaPort::CMD_SCSILINK_SETMODE:
			dayna_port->SetMode(ctrl.cmd, ctrl.buffer);
			Status();
			break;
		break;
		case SCSIDaynaPort::CMD_SCSILINK_SETMAC:
			ctrl.length = 6;
			// Write phase
			DataOut();
			break;
		default:
			LOGWARN("%s Unknown SetInterface command received: %02X", __PRETTY_FUNCTION__, (unsigned int)ctrl.cmd[5]);
	}
}

//---------------------------------------------------------------------------
//
// 	Set the multicast address
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdSetMcastAddr()
{
	ASSERT(this);

	LOGTRACE("%s Set Multicast Address Command ", __PRETTY_FUNCTION__);

	DWORD lun = GetLun();

	if (ctrl.unit[lun]->GetID() != MAKEID('S', 'C', 'D', 'P')){
		LOGWARN("Received a SetMcastAddress command for a non-daynaport unit");
		Error();
		return;
	}

	// Command processing on drive
	ctrl.length = (DWORD)ctrl.cmd[4];
	
	// ASSERT(ctrl.length >= 0);
	if (ctrl.length == 0) {
		LOGWARN("%s Not supported SetMcastAddr Command %02X", __PRETTY_FUNCTION__, (WORD)ctrl.cmd[2]);

		// Failure (Error)
		Error();
		return;
	}

	DataOut();
}

//---------------------------------------------------------------------------
//
// 	Enable/disable Interface (0e)
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdEnableInterface()
{
	BOOL status;
	SCSIDaynaPort *dayna_port;

	ASSERT(this);

	LOGTRACE("%s",__PRETTY_FUNCTION__);

	DWORD lun = GetLun();

	// Error if not a DaynaPort SCSI Link
	if (ctrl.unit[lun]->GetID() != MAKEID('S', 'C', 'D', 'P')){
		LOGWARN("%s Received a CmdRetrieveStats command for a non-daynaport unit %08X", __PRETTY_FUNCTION__, (unsigned int)ctrl.unit[lun]->GetID());
		Error();
		return;
	}

	dayna_port = (SCSIDaynaPort*)ctrl.unit[lun];

	// Command processing on drive
	status = dayna_port->EnableInterface(ctrl.cmd);
	if (!status) {
		// Failure (Error)
		Error();
		return;
	}

	// status phase
	Status();
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
		LOGTRACE("%s sending handhake with offset %lu, length %lu", __PRETTY_FUNCTION__, ctrl.offset, ctrl.length);

		// The Daynaport needs to have a delay after the size/flags field
		// of the read response. In the MacOS driver, it looks like the
		// driver is doing two "READ" system calls.
		if (ctrl.unit[0] && ctrl.unit[0]->GetID() == MAKEID('S', 'C', 'D', 'P')) {
			len = ((GPIOBUS*)ctrl.bus)->SendHandShake(
				&ctrl.buffer[ctrl.offset], ctrl.length, SCSIDaynaPort::DAYNAPORT_READ_HEADER_SZ);
		}
		else
		{
			len = ctrl.bus->SendHandShake(
				&ctrl.buffer[ctrl.offset], ctrl.length, BUS::SEND_NO_DELAY);
		}

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
			// set next buffer (set offset, length)
			result = XferIn(ctrl.buffer);
			LOGTRACE("%s processing after data collection. Blocks: %lu", __PRETTY_FUNCTION__, ctrl.blocks);
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
		LOGTRACE("%s Continuing to send. blocks = %lu", __PRETTY_FUNCTION__, ctrl.blocks);
		ASSERT(ctrl.length > 0);
		ASSERT(ctrl.offset == 0);
		#ifndef RASCSI
		// Signal line operated by the target
		ctrl.bus->SetREQ(TRUE);
		#endif	// RASCSI
		return;
	}

	// Move to next phase
	LOGTRACE("%s Move to next phase %s (%d)", __PRETTY_FUNCTION__, BUS::GetPhaseStrRaw(ctrl.phase), ctrl.phase);
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
	LOGTRACE("%s ctrl.phase: %d (%s)",__PRETTY_FUNCTION__, (int)ctrl.phase, BUS::GetPhaseStrRaw(ctrl.phase));
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
                        try {
                        	Execute();
                        }
                        catch (const lunexception& e) {
                            LOGINFO("%s unsupported LUN %d", __PRETTY_FUNCTION__, (int)e.getlun());
                            // LOGICAL UNIT NOT SUPPORTED
                            Error(0x05, 0x25);
                        }
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

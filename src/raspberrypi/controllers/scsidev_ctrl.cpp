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
#include <sstream>

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
SCSIDEV::SCSIDEV() : SASIDEV()
{
	// Synchronous transfer work initialization
	scsi.syncenable = FALSE;
	scsi.syncperiod = 50;
	scsi.syncoffset = 0;
	scsi.atnmsg = FALSE;
	scsi.msc = 0;
	memset(scsi.msb, 0x00, sizeof(scsi.msb));

	SetUpCommand(eCmdTestUnitReady, "CmdTestUnitReady", &SCSIDEV::CmdTestUnitReady);
	SetUpCommand(eCmdRezero, "CmdRezero", &SCSIDEV::CmdRezero);
	SetUpCommand(eCmdRequestSense, "CmdRequestSense", &SCSIDEV::CmdRequestSense);
	SetUpCommand(eCmdFormat, "CmdFormat", &SCSIDEV::CmdFormat);
	SetUpCommand(eCmdReassign, "CmdReassign", &SCSIDEV::CmdReassign);
	SetUpCommand(eCmdRead6, "CmdRead6", &SCSIDEV::CmdRead6);
	SetUpCommand(eCmdWrite6, "CmdWrite6", &SCSIDEV::CmdWrite6);
	SetUpCommand(eCmdSeek6, "CmdSeek6", &SCSIDEV::CmdSeek6);
	SetUpCommand(eCmdInquiry, "CmdInquiry", &SCSIDEV::CmdInquiry);
	SetUpCommand(eCmdModeSelect, "CmdModeSelect", &SCSIDEV::CmdModeSelect);
	SetUpCommand(eCmdReserve6, "CmdReserve6", &SCSIDEV::CmdReserve6);
	SetUpCommand(eCmdRelease6, "CmdRelease6", &SCSIDEV::CmdRelease6);
	SetUpCommand(eCmdModeSense, "CmdModeSense", &SCSIDEV::CmdModeSense);
	SetUpCommand(eCmdStartStop, "CmdStartStop", &SCSIDEV::CmdStartStop);
	SetUpCommand(eCmdSendDiag, "CmdSendDiag", &SCSIDEV::CmdSendDiag);
	SetUpCommand(eCmdRemoval, "CmdRemoval", &SCSIDEV::CmdRemoval);
	SetUpCommand(eCmdReadCapacity10, "CmdReadCapacity10", &SCSIDEV::CmdReadCapacity10);
	SetUpCommand(eCmdRead10, "CmdRead10", &SCSIDEV::CmdRead10);
	SetUpCommand(eCmdWrite10, "CmdWrite10", &SCSIDEV::CmdWrite10);
	SetUpCommand(eCmdVerify10, "CmdVerify10", &SCSIDEV::CmdWrite10);
	SetUpCommand(eCmdSeek10, "CmdSeek10", &SCSIDEV::CmdSeek10);
	SetUpCommand(eCmdVerify, "CmdVerify", &SCSIDEV::CmdVerify);
	SetUpCommand(eCmdSynchronizeCache, "CmdSynchronizeCache", &SCSIDEV::CmdSynchronizeCache);
	SetUpCommand(eCmdReadDefectData10, "CmdReadDefectData10", &SCSIDEV::CmdReadDefectData10);
	SetUpCommand(eCmdModeSelect10, "CmdModeSelect10", &SCSIDEV::CmdModeSelect10);
	SetUpCommand(eCmdReserve10, "CmdReserve10", &SCSIDEV::CmdReserve10);
	SetUpCommand(eCmdRelease10, "CmdRelease10", &SCSIDEV::CmdRelease10);
	SetUpCommand(eCmdModeSense10, "CmdModeSense10", &SCSIDEV::CmdModeSense10);
	SetUpCommand(eCmdRead16, "CmdRead16", &SCSIDEV::CmdRead16);
	SetUpCommand(eCmdWrite16, "CmdWrite16", &SCSIDEV::CmdWrite16);
	SetUpCommand(eCmdVerify16, "CmdVerify16", &SCSIDEV::CmdWrite16);
	SetUpCommand(eCmdReadCapacity16, "CmdReadCapacity16", &SCSIDEV::CmdReadCapacity16);
	SetUpCommand(eCmdReportLuns, "CmdReportLuns", &SCSIDEV::CmdReportLuns);

	// MMC specific. TODO Move to separate class
	SetUpCommand(eCmdReadToc, "CmdReadToc", &SCSIDEV::CmdReadToc);
	SetUpCommand(eCmdPlayAudio10, "CmdPlayAudio10", &SCSIDEV::CmdPlayAudio10);
	SetUpCommand(eCmdPlayAudioMSF, "CmdPlayAudioMSF", &SCSIDEV::CmdPlayAudioMSF);
	SetUpCommand(eCmdPlayAudioTrack, "CmdPlayAudioTrack", &SCSIDEV::CmdPlayAudioTrack);
	SetUpCommand(eCmdGetEventStatusNotification, "CmdGetEventStatusNotification", &SCSIDEV::CmdGetEventStatusNotification);

	// DaynaPort specific. TODO Move to separate class
	SetUpCommand(eCmdRetrieveStats, "CmdRetrieveStats", &SCSIDEV::CmdRetrieveStats);
	SetUpCommand(eCmdSetIfaceMode, "CmdSetIfaceMode", &SCSIDEV::CmdSetIfaceMode);
	SetUpCommand(eCmdSetMcastAddr, "CmdSetMcastAddr", &SCSIDEV::CmdSetMcastAddr);
	SetUpCommand(eCmdEnableInterface, "CmdEnableInterface", &SCSIDEV::CmdEnableInterface);
}

SCSIDEV::~SCSIDEV()
{
	for (auto const& command : scsi_commands) {
		free(command.second);
	}
}

void SCSIDEV::SetUpCommand(scsi_command opcode, const char* name, void (SCSIDEV::*execute)(void))
{
	scsi_commands[opcode] = new command_t(name, execute);
}

//---------------------------------------------------------------------------
//
//	Device reset
//
//---------------------------------------------------------------------------
void SCSIDEV::Reset()
{
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
BUS::phase_t SCSIDEV::Process()
{
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
void SCSIDEV::BusFree()
{
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
		ctrl.bus->SetBSY(false);

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
void SCSIDEV::Selection()
{
	// Phase change
	if (ctrl.phase != BUS::selection) {
		// invalid if IDs do not match
		DWORD id = 1 << ctrl.m_scsi_id;
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
		ctrl.bus->SetBSY(true);
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
void SCSIDEV::Execute()
{
	LOGTRACE("%s Execution phase command $%02X", __PRETTY_FUNCTION__, (unsigned int)ctrl.cmd[0]);

	// Phase Setting
	ctrl.phase = BUS::execute;

	// Initialization for data transfer
	ctrl.offset = 0;
	ctrl.blocks = 1;
	ctrl.execstart = SysTimer::GetTimerLow();

	// If the command is valid it must be contained in the command map
	if (!scsi_commands.count(static_cast<scsi_command>(ctrl.cmd[0]))) {
		CmdInvalid();
		return;
	}

	command_t* command = scsi_commands[static_cast<scsi_command>(ctrl.cmd[0])];

	LOGDEBUG("++++ CMD ++++ %s ID %d received %s ($%02X)", __PRETTY_FUNCTION__, GetSCSIID(), command->name, (unsigned int)ctrl.cmd[0]);

	// Process by command
	(this->*command->execute)();
}

//---------------------------------------------------------------------------
//
//	Message out phase
//
//---------------------------------------------------------------------------
void SCSIDEV::MsgOut()
{
	LOGTRACE("%s ID %d",__PRETTY_FUNCTION__, GetSCSIID());

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
void SCSIDEV::Error(ERROR_CODES::sense_key sense_key, ERROR_CODES::asc asc)
{
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

	// Logical Unit
	DWORD lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun] || asc == ERROR_CODES::INVALID_LUN) {
		lun = 0;
	}

	LOGTRACE("%s Sense Key and ASC for subsequent REQUEST SENSE: $%02X, $%02X", __PRETTY_FUNCTION__, sense_key, asc);

	if (sense_key || asc) {
		// Set Sense Key and ASC for a subsequent REQUEST SENSE
		ctrl.unit[lun]->SetStatusCode((sense_key << 16) | (asc << 8));
	}

	// Set status (CHECK CONDITION only for valid LUNs for non-REQUEST SENSE)
	ctrl.status = (ctrl.cmd[0] == eCmdRequestSense && asc == ERROR_CODES::asc::INVALID_LUN) ? 0x00 : 0x02;

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
void SCSIDEV::CmdInquiry()
{
	LOGTRACE("%s INQUIRY Command", __PRETTY_FUNCTION__);

	// Find a valid unit
	// TODO The code below is probably wrong. It results in the same INQUIRY data being
	// used for all LUNs, even though each LUN has its individual set of INQUIRY data.
	PrimaryDevice *device = NULL;
	for (int valid_lun = 0; valid_lun < UnitMax; valid_lun++) {
		if (ctrl.unit[valid_lun]) {
			device = ctrl.unit[valid_lun];
			break;
		}
	}

	// Processed on the disk side (it is originally processed by the controller)
	if (device) {
		ctrl.length = device->Inquiry(ctrl.cmd, ctrl.buffer);
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
void SCSIDEV::CmdModeSelect()
{
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
void SCSIDEV::CmdReserve6()
{
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
void SCSIDEV::CmdReserve10()
{
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
void SCSIDEV::CmdRelease6()
{
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
void SCSIDEV::CmdRelease10()
{
	LOGTRACE( "%s Release(10) Command", __PRETTY_FUNCTION__);

	// status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	MODE SENSE
//
//---------------------------------------------------------------------------
void SCSIDEV::CmdModeSense()
{
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
void SCSIDEV::CmdStartStop()
{
	LOGTRACE( "%s START STOP UNIT Command ", __PRETTY_FUNCTION__);

	DWORD lun = GetLun();

	// Command processing on drive
	bool status = ctrl.unit[lun]->StartStop(ctrl.cmd);
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
void SCSIDEV::CmdSendDiag()
{
	LOGTRACE( "%s SEND DIAGNOSTIC Command ", __PRETTY_FUNCTION__);

	DWORD lun = GetLun();

	// Command processing on drive
	bool status = ctrl.unit[lun]->SendDiag(ctrl.cmd);
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
void SCSIDEV::CmdRemoval()
{
	LOGTRACE( "%s PREVENT/ALLOW MEDIUM REMOVAL Command ", __PRETTY_FUNCTION__);

	DWORD lun = GetLun();

	// Command processing on drive
	bool status = ctrl.unit[lun]->Removal(ctrl.cmd);
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
void SCSIDEV::CmdReadCapacity10()
{
	LOGTRACE( "%s READ CAPACITY(10) Command ", __PRETTY_FUNCTION__);

	DWORD lun = GetLun();

	// Command processing on drive
	int length = ctrl.unit[lun]->ReadCapacity10(ctrl.cmd, ctrl.buffer);
	if (length <= 0) {
		Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::MEDIUM_NOT_PRESENT);
		return;
	}

	// Length setting
	ctrl.length = length;

	// Data-in Phase
	DataIn();
}

void SCSIDEV::CmdReadCapacity16()
{
	LOGTRACE( "%s READ CAPACITY(16) Command ", __PRETTY_FUNCTION__);

	DWORD lun = GetLun();

	// Command processing on drive
	int length = ctrl.unit[lun]->ReadCapacity16(ctrl.cmd, ctrl.buffer);
	if (length <= 0) {
		Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::MEDIUM_NOT_PRESENT);
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
void SCSIDEV::CmdRead10()
{
	DWORD lun = GetLun();

	// Receive message if host bridge
	if (ctrl.unit[lun]->IsBridge()) {
		CmdGetMessage10();
		return;
	}

	// Get record number and block number
	DWORD record = ctrl.cmd[2];
	record <<= 8;
	record |= ctrl.cmd[3];
	record <<= 8;
	record |= ctrl.cmd[4];
	record <<= 8;
	record |= ctrl.cmd[5];
	ctrl.blocks = ctrl.cmd[7];
	ctrl.blocks <<= 8;
	ctrl.blocks |= ctrl.cmd[8];

	// Check capacity
	DWORD capacity = ctrl.unit[lun]->GetBlockCount();
	if (record > capacity || record + ctrl.blocks > capacity) {
		ostringstream s;
		s << "ID " << GetSCSIID() << ": Media capacity of " << capacity << " blocks exceeded: "
				<< "Trying to read block " << record << ", block count " << ctrl.blocks;
		LOGWARN("%s", s.str().c_str());
		Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::LBA_OUT_OF_RANGE);
		return;
	}

	LOGTRACE("%s READ(10) command record=%d blocks=%d", __PRETTY_FUNCTION__, (unsigned int)record, (int)ctrl.blocks);

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
//	READ(16)
//
//---------------------------------------------------------------------------
void SCSIDEV::CmdRead16()
{
	DWORD lun = GetLun();

	// Receive message if host bridge
	if (ctrl.unit[lun]->IsBridge()) {
		Error();
		return;
	}

	// Report an error as long as big drives are not supported
	if (ctrl.cmd[2] || ctrl.cmd[3] || ctrl.cmd[4] || ctrl.cmd[5]) {
		LOGWARN("Can't execute READ(16) with 64 bit sector number");
		Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::LBA_OUT_OF_RANGE);
		return;
	}

	// Get record number and block number
	DWORD record = ctrl.cmd[6];
	record <<= 8;
	record |= ctrl.cmd[7];
	record <<= 8;
	record |= ctrl.cmd[8];
	record <<= 8;
	record |= ctrl.cmd[9];
	ctrl.blocks = ctrl.cmd[10];
	ctrl.blocks <<= 8;
	ctrl.blocks |= ctrl.cmd[11];
	ctrl.blocks <<= 8;
	ctrl.blocks |= ctrl.cmd[12];
	ctrl.blocks <<= 8;
	ctrl.blocks |= ctrl.cmd[13];

	// Check capacity
	DWORD capacity = ctrl.unit[lun]->GetBlockCount();
	if (record > capacity || record + ctrl.blocks > capacity) {
		ostringstream s;
		s << "ID " << GetSCSIID() << ": Media capacity of " << capacity << " blocks exceeded: "
				<< "Trying to read block " << record << ", block count " << ctrl.blocks;
		LOGWARN("%s", s.str().c_str());
		Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::LBA_OUT_OF_RANGE);
		return;
	}

	LOGTRACE("%s READ(16) command record=%d blocks=%d", __PRETTY_FUNCTION__, (unsigned int)record, (int)ctrl.blocks);

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
void SCSIDEV::CmdWrite10()
{
	DWORD lun = GetLun();

	// Receive message with host bridge
	if (ctrl.unit[lun]->IsBridge()) {
		CmdSendMessage10();
		return;
	}

	// Get record number and block number
	DWORD record = ctrl.cmd[2];
	record <<= 8;
	record |= ctrl.cmd[3];
	record <<= 8;
	record |= ctrl.cmd[4];
	record <<= 8;
	record |= ctrl.cmd[5];
	ctrl.blocks = ctrl.cmd[7];
	ctrl.blocks <<= 8;
	ctrl.blocks |= ctrl.cmd[8];

	// Check capacity
	DWORD capacity = ctrl.unit[lun]->GetBlockCount();
	if (record > capacity || record + ctrl.blocks > capacity) {
		ostringstream s;
		s << "ID " << GetSCSIID() << ": Media capacity of " << capacity << " blocks exceeded: "
				<< "Trying to read block " << record << ", block count " << ctrl.blocks;
		LOGWARN("%s", s.str().c_str());
		Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::LBA_OUT_OF_RANGE);
		return;
	}

	LOGTRACE("%s WRITE(10) command record=%d blocks=%d",__PRETTY_FUNCTION__, (unsigned int)record, (unsigned int)ctrl.blocks);

	// Do not process 0 blocks
	if (ctrl.blocks == 0) {
		Status();
		return;
	}

	// Command processing on drive
	ctrl.length = ctrl.unit[lun]->WriteCheck(record);
	if (ctrl.length <= 0) {
		// Failure (Error)
		Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::WRITE_PROTECTED);
		return;
	}

	// Set next block
	ctrl.next = record + 1;

	// Data out phase
	DataOut();
}

//---------------------------------------------------------------------------
//
//	WRITE(16)
//
//---------------------------------------------------------------------------
void SCSIDEV::CmdWrite16()
{
	DWORD lun = GetLun();

	// Receive message if host bridge
	if (ctrl.unit[lun]->IsBridge()) {
		Error();
		return;
	}

	// Report an error as long as big drives are not supported
	if (ctrl.cmd[2] || ctrl.cmd[3] || ctrl.cmd[4] || ctrl.cmd[5]) {
		LOGWARN("Can't execute WRITE(16) with 64 bit sector number");
		Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::LBA_OUT_OF_RANGE);
		return;
	}

	// Get record number and block number
	DWORD record = ctrl.cmd[6];
	record <<= 8;
	record |= ctrl.cmd[7];
	record <<= 8;
	record |= ctrl.cmd[8];
	record <<= 8;
	record |= ctrl.cmd[9];
	ctrl.blocks = ctrl.cmd[10];
	ctrl.blocks <<= 8;
	ctrl.blocks |= ctrl.cmd[11];
	ctrl.blocks <<= 8;
	ctrl.blocks |= ctrl.cmd[12];
	ctrl.blocks <<= 8;
	ctrl.blocks |= ctrl.cmd[13];

	// Check capacity
	DWORD capacity = ctrl.unit[lun]->GetBlockCount();
	if (record > capacity || record + ctrl.blocks > capacity) {
		ostringstream s;
		s << "ID " << GetSCSIID() << ": Media capacity of " << capacity << " blocks exceeded: "
				<< "Trying to read block " << record << ", block count " << ctrl.blocks;
		LOGWARN("%s", s.str().c_str());
		Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::LBA_OUT_OF_RANGE);
		return;
	}

	LOGTRACE("%s WRITE(16) command record=%d blocks=%d",__PRETTY_FUNCTION__, (unsigned int)record, (unsigned int)ctrl.blocks);

	// Do not process 0 blocks
	if (ctrl.blocks == 0) {
		Status();
		return;
	}

	// Command processing on drive
	ctrl.length = ctrl.unit[lun]->WriteCheck(record);
	if (ctrl.length <= 0) {
		// Failure (Error)
		Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::WRITE_PROTECTED);
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
void SCSIDEV::CmdSeek10()
{
	LOGTRACE( "%s SEEK(10) Command ", __PRETTY_FUNCTION__);

	DWORD lun = GetLun();

	// Command processing on drive
	BOOL status = ctrl.unit[lun]->Seek(ctrl.cmd);
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
void SCSIDEV::CmdVerify()
{
	BOOL status;

	DWORD lun = GetLun();

	// Get record number and block number
	DWORD record = ctrl.cmd[2];
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
//	REPORT LUNS
//
//---------------------------------------------------------------------------
void SCSIDEV::CmdReportLuns()
{
	DWORD lun = GetLun();

	int length = ctrl.unit[lun]->ReportLuns(ctrl.cmd, ctrl.buffer);
	if (length <= 0) {
		// Failure (Error)
		Error();
		return;
	}

	ctrl.length = length;

	// Data in phase
	DataIn();
}

//---------------------------------------------------------------------------
//
//	SYNCHRONIZE CACHE
//
//---------------------------------------------------------------------------
void SCSIDEV::CmdSynchronizeCache()
{
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
void SCSIDEV::CmdReadDefectData10()
{
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
void SCSIDEV::CmdReadToc()
{
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
void SCSIDEV::CmdPlayAudio10()
{
	DWORD lun = GetLun();

	// Command processing on drive
	bool status = ctrl.unit[lun]->PlayAudio(ctrl.cmd);
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
void SCSIDEV::CmdPlayAudioMSF()
{
	DWORD lun = GetLun();

	// Command processing on drive
	bool status = ctrl.unit[lun]->PlayAudioMSF(ctrl.cmd);
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
void SCSIDEV::CmdPlayAudioTrack()
{
	DWORD lun = GetLun();

	// Command processing on drive
	bool status = ctrl.unit[lun]->PlayAudioTrack(ctrl.cmd);
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
//	GET EVENT STATUS NOTIFICATION
//
//---------------------------------------------------------------------------
void SCSIDEV::CmdGetEventStatusNotification()
{
	GetLun();

	// This naive (but legal) implementation avoids constant warnings in the logs
	Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::INVALID_FIELD_IN_CDB);
}

//---------------------------------------------------------------------------
//
//	MODE SELECT10
//
//---------------------------------------------------------------------------
void SCSIDEV::CmdModeSelect10()
{
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
void SCSIDEV::CmdModeSense10()
{
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
void SCSIDEV::CmdGetMessage10()
{
	SCSIBR *bridge;

	DWORD lun = GetLun();

	// Error if not a host bridge
	if (!ctrl.unit[lun]->IsBridge()) {
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
void SCSIDEV::CmdSendMessage10()
{
	DWORD lun = GetLun();

	// Error if not a host bridge
	if (!ctrl.unit[lun]->IsBridge()) {
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
void SCSIDEV::CmdRetrieveStats()
{
	DWORD lun = GetLun();

	// Error if not a DaynaPort SCSI Link
	if (!ctrl.unit[lun]->IsDaynaPort()) {
		LOGWARN("Received a CmdRetrieveStats command for a non-daynaport unit %s", ctrl.unit[lun]->GetType().c_str());
		Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::INVALID_COMMAND_OPERATION_CODE);
		return;
	}

	// Process with drive
	SCSIDaynaPort *dayna_port = (SCSIDaynaPort*)ctrl.unit[lun];
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
void SCSIDEV::CmdSetIfaceMode()
{
	LOGTRACE("%s",__PRETTY_FUNCTION__);

	DWORD lun = GetLun();

	// Error if not a DaynaPort SCSI Link
	if (!ctrl.unit[lun]->IsDaynaPort()) {
		LOGWARN("%s Received a CmdSetIfaceMode command for a non-daynaport unit %s", __PRETTY_FUNCTION__, ctrl.unit[lun]->GetType().c_str());
		Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::INVALID_COMMAND_OPERATION_CODE);
		return;
	}

	SCSIDaynaPort *dayna_port = (SCSIDaynaPort*)ctrl.unit[lun];

	// Check whether this command is telling us to "Set Interface Mode" or "Set MAC Address"

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
void SCSIDEV::CmdSetMcastAddr()
{
	LOGTRACE("%s Set Multicast Address Command ", __PRETTY_FUNCTION__);

	DWORD lun = GetLun();

	if (!ctrl.unit[lun]->IsDaynaPort()) {
		LOGWARN("Received a SetMcastAddress command for a non-daynaport unit");
		Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::INVALID_COMMAND_OPERATION_CODE);
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
void SCSIDEV::CmdEnableInterface()
{
	LOGTRACE("%s",__PRETTY_FUNCTION__);

	DWORD lun = GetLun();

	// Error if not a DaynaPort SCSI Link
	if (!ctrl.unit[lun]->IsDaynaPort()) {
		LOGWARN("%s Received a CmdEnableInterface command for a non-daynaport unit %s", __PRETTY_FUNCTION__, ctrl.unit[lun]->GetType().c_str());
		Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::INVALID_COMMAND_OPERATION_CODE);
		return;
	}

	SCSIDaynaPort *dayna_port = (SCSIDaynaPort*)ctrl.unit[lun];

	// Command processing on drive
	BOOL status = dayna_port->EnableInterface(ctrl.cmd);
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
void SCSIDEV::Send()
{
	int len;
	BOOL result;

	ASSERT(!ctrl.bus->GetREQ());
	ASSERT(ctrl.bus->GetIO());

	//if Length! = 0, send
	if (ctrl.length != 0) {
		ostringstream s;
		s << __PRETTY_FUNCTION__ << " sending handhake with offset " << ctrl.offset << ", length " << ctrl.length;
		LOGTRACE("%s", s.str().c_str());

		// The Daynaport needs to have a delay after the size/flags field
		// of the read response. In the MacOS driver, it looks like the
		// driver is doing two "READ" system calls.
		if (ctrl.unit[0] && ctrl.unit[0]->IsDaynaPort()) {
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

	// Block subtraction, result initialization
	ctrl.blocks--;
	result = TRUE;

	// Processing after data collection (read/data-in only)
	if (ctrl.phase == BUS::datain) {
		if (ctrl.blocks != 0) {
			// set next buffer (set offset, length)
			result = XferIn(ctrl.buffer);
			ostringstream s;
			s << __PRETTY_FUNCTION__ << " processing after data collection. Blocks: " << ctrl.blocks;
			LOGTRACE("%s", s.str().c_str());
		}
	}

	// If result FALSE, move to status phase
	if (!result) {
		Error();
		return;
	}

	// Continue sending if block !=0
	if (ctrl.blocks != 0){
		ostringstream s;
		s << __PRETTY_FUNCTION__ << " Continuing to send. blocks = " << ctrl.blocks;
		LOGTRACE("%s", s.str().c_str());
		ASSERT(ctrl.length > 0);
		ASSERT(ctrl.offset == 0);
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

//---------------------------------------------------------------------------
//
//  Receive Data
//
//---------------------------------------------------------------------------
void SCSIDEV::Receive()
{
	int len;
	BYTE data;

	LOGTRACE("%s",__PRETTY_FUNCTION__);

	// REQ is low
	ASSERT(!ctrl.bus->GetREQ());
	ASSERT(!ctrl.bus->GetIO());

	// Length != 0 if received
	if (ctrl.length != 0) {
		LOGTRACE("%s length was != 0", __PRETTY_FUNCTION__);
		// Receive
		len = ctrl.bus->ReceiveHandShake(&ctrl.buffer[ctrl.offset], ctrl.length);

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
	BOOL result = TRUE;

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
			len = GPIOBUS::GetCommandByteCount(ctrl.buffer[0]);

			for (int i = 0; i < len; i++) {
				ctrl.cmd[i] = (DWORD)ctrl.buffer[i];
				LOGTRACE("%s Command $%02X",__PRETTY_FUNCTION__, ctrl.cmd[i]);
			}

			// Execution Phase
			try {
				Execute();
			}
			catch (const lun_exception& e) {
				LOGINFO("%s Invalid LUN %d", __PRETTY_FUNCTION__, e.getlun());

				Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::INVALID_LUN);
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
				int i = 0;
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
BOOL SCSIDEV::XferMsg(DWORD msg)
{
	ASSERT(ctrl.phase == BUS::msgout);

	// Save message out data
	if (scsi.atnmsg) {
		scsi.msb[scsi.msc] = (BYTE)msg;
		scsi.msc++;
		scsi.msc %= 256;
	}

	return TRUE;
}

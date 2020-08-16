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
//  [ SASI device controller ]
//
//---------------------------------------------------------------------------
#include "sasidev_ctrl.h"
#include "filepath.h"
#include "gpiobus.h"
#include "scsi_host_bridge.h"

//===========================================================================
//
//	SASI Device
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
#ifdef RASCSI
SASIDEV::SASIDEV()
#else
SASIDEV::SASIDEV(Device *dev)
#endif	// RASCSI
{
	int i;

#ifndef RASCSI
	// Remember host device
	host = dev;
#endif	// RASCSI

	// Work initialization
	ctrl.phase = BUS::busfree;
	ctrl.id = -1;
	ctrl.bus = NULL;
	memset(ctrl.cmd, 0x00, sizeof(ctrl.cmd));
	ctrl.status = 0x00;
	ctrl.message = 0x00;
#ifdef RASCSI
	ctrl.execstart = 0;
#endif	// RASCSI
	ctrl.bufsize = 0x800;
	ctrl.buffer = (BYTE *)malloc(ctrl.bufsize);
	memset(ctrl.buffer, 0x00, ctrl.bufsize);
	ctrl.blocks = 0;
	ctrl.next = 0;
	ctrl.offset = 0;
	ctrl.length = 0;

	// Logical unit initialization
	for (i = 0; i < UnitMax; i++) {
		ctrl.unit[i] = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
SASIDEV::~SASIDEV()
{
	// Free the buffer
	if (ctrl.buffer) {
		free(ctrl.buffer);
		ctrl.buffer = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	Device reset
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::Reset()
{
	int i;

	ASSERT(this);

	// Work initialization
	memset(ctrl.cmd, 0x00, sizeof(ctrl.cmd));
	ctrl.phase = BUS::busfree;
	ctrl.status = 0x00;
	ctrl.message = 0x00;
#ifdef RASCSI
	ctrl.execstart = 0;
#endif	// RASCSI
	memset(ctrl.buffer, 0x00, ctrl.bufsize);
	ctrl.blocks = 0;
	ctrl.next = 0;
	ctrl.offset = 0;
	ctrl.length = 0;

	// Unit initialization
	for (i = 0; i < UnitMax; i++) {
		if (ctrl.unit[i]) {
			ctrl.unit[i]->Reset();
		}
	}
}

#ifndef RASCSI
//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL SASIDEV::Save(Fileio *fio, int /*ver*/)
{
	DWORD sz;

	ASSERT(this);
	ASSERT(fio);

	// Save size
	sz = 2120;
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// Save entity
	PROP_EXPORT(fio, ctrl.phase);
	PROP_EXPORT(fio, ctrl.id);
	PROP_EXPORT(fio, ctrl.cmd);
	PROP_EXPORT(fio, ctrl.status);
	PROP_EXPORT(fio, ctrl.message);
	if (!fio->Write(ctrl.buffer, 0x800)) {
		return FALSE;
	}
	PROP_EXPORT(fio, ctrl.blocks);
	PROP_EXPORT(fio, ctrl.next);
	PROP_EXPORT(fio, ctrl.offset);
	PROP_EXPORT(fio, ctrl.length);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL SASIDEV::Load(Fileio *fio, int ver)
{
	DWORD sz;

	ASSERT(this);
	ASSERT(fio);

	// Not saved before version 3.11
	if (ver <= 0x0311) {
		return TRUE;
	}

	// Load size and check if the size matches
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != 2120) {
		return FALSE;
	}

	// Load the entity
	PROP_IMPORT(fio, ctrl.phase);
	PROP_IMPORT(fio, ctrl.id);
	PROP_IMPORT(fio, ctrl.cmd);
	PROP_IMPORT(fio, ctrl.status);
	PROP_IMPORT(fio, ctrl.message);
	if (!fio->Read(ctrl.buffer, 0x800)) {
		return FALSE;
	}
	PROP_IMPORT(fio, ctrl.blocks);
	PROP_IMPORT(fio, ctrl.next);
	PROP_IMPORT(fio, ctrl.offset);
	PROP_IMPORT(fio, ctrl.length);

	return TRUE;
}
#endif	// RASCSI

//---------------------------------------------------------------------------
//
//	Connect the controller
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::Connect(int id, BUS *bus)
{
	ASSERT(this);

	ctrl.id = id;
	ctrl.bus = bus;
}

//---------------------------------------------------------------------------
//
//	Get the logical unit
//
//---------------------------------------------------------------------------
Disk* FASTCALL SASIDEV::GetUnit(int no)
{
	ASSERT(this);
	ASSERT(no < UnitMax);

	return ctrl.unit[no];
}

//---------------------------------------------------------------------------
//
//	Set the logical unit
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::SetUnit(int no, Disk *dev)
{
	ASSERT(this);
	ASSERT(no < UnitMax);

	ctrl.unit[no] = dev;
}

//---------------------------------------------------------------------------
//
//	Check to see if this has a valid logical unit
//
//---------------------------------------------------------------------------
BOOL FASTCALL SASIDEV::HasUnit()
{
	int i;

	ASSERT(this);

	for (i = 0; i < UnitMax; i++) {
		if (ctrl.unit[i]) {
			return TRUE;
		}
	}

	return FALSE;
}

//---------------------------------------------------------------------------
//
//	Get internal data
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::GetCTRL(ctrl_t *buffer)
{
	ASSERT(this);
	ASSERT(buffer);

	// reference the internal structure
	*buffer = ctrl;
}

//---------------------------------------------------------------------------
//
//	Get a busy unit
//
//---------------------------------------------------------------------------
Disk* FASTCALL SASIDEV::GetBusyUnit()
{
	DWORD lun;

	ASSERT(this);

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	return ctrl.unit[lun];
}

//---------------------------------------------------------------------------
//
//	Run
//
//---------------------------------------------------------------------------
BUS::phase_t FASTCALL SASIDEV::Process()
{
	ASSERT(this);

	// Do nothing if not connected
	if (ctrl.id < 0 || ctrl.bus == NULL) {
		return ctrl.phase;
	}

	// Get bus information
	ctrl.bus->Aquire();

	// For the monitor tool, we shouldn't need to reset. We're just logging information
	// Reset
	if (ctrl.bus->GetRST()) {
#if defined(DISK_LOG)
		Log(Log::Normal, "RESET signal received");
#endif	// DISK_LOG

		// Reset the controller
		Reset();

		// Reset the bus
		ctrl.bus->Reset();
		return ctrl.phase;
	}

	// Phase processing
	switch (ctrl.phase) {
		// Bus free
		case BUS::busfree:
			BusFree();
			break;

		// Selection
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

		// Msg in (MCI=111)
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
//	Bus free phase
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::BusFree()
{
	ASSERT(this);

	// Phase change
	if (ctrl.phase != BUS::busfree) {

#if defined(DISK_LOG)
		Log(Log::Normal, "Bus free phase");
#endif	// DISK_LOG

		// Phase Setting
		ctrl.phase = BUS::busfree;

		// Set Signal lines
		ctrl.bus->SetREQ(FALSE);
		ctrl.bus->SetMSG(FALSE);
		ctrl.bus->SetCD(FALSE);
		ctrl.bus->SetIO(FALSE);
		ctrl.bus->SetBSY(FALSE);

		// Initialize status and message
		ctrl.status = 0x00;
		ctrl.message = 0x00;
		return;
	}

	// Move to selection phase
	if (ctrl.bus->GetSEL() && !ctrl.bus->GetBSY()) {
		Selection();
	}
}

//---------------------------------------------------------------------------
//
//	Selection phase
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::Selection()
{
	DWORD id;

	ASSERT(this);

	// Phase change
	if (ctrl.phase != BUS::selection) {
		// Invalid if IDs do not match
		id = 1 << ctrl.id;
		if ((ctrl.bus->GetDAT() & id) == 0) {
			return;
		}

		// Return if there is no unit
		if (!HasUnit()) {
			return;
		}

#if defined(DISK_LOG)
		Log(Log::Normal,
			"Selection Phase ID=%d (with device)", ctrl.id);
#endif	// DISK_LOG

		// Phase change
		ctrl.phase = BUS::selection;

		// Raiase BSY and respond
		ctrl.bus->SetBSY(TRUE);
		return;
	}

	// Command phase shifts when selection is completed
	if (!ctrl.bus->GetSEL() && ctrl.bus->GetBSY()) {
		Command();
	}
}

//---------------------------------------------------------------------------
//
//	Command phase
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::Command()
{
#ifdef RASCSI
	int count;
	int i;
#endif	// RASCSI

	ASSERT(this);

	// Phase change
	if (ctrl.phase != BUS::command) {

#if defined(DISK_LOG)
		Log(Log::Normal, "Command Phase");
#endif	// DISK_LOG

		// Phase Setting
		ctrl.phase = BUS::command;

		// Signal line operated by the target
		ctrl.bus->SetMSG(FALSE);
		ctrl.bus->SetCD(TRUE);
		ctrl.bus->SetIO(FALSE);

		// Data transfer is 6 bytes x 1 block
		ctrl.offset = 0;
		ctrl.length = 6;
		ctrl.blocks = 1;

#ifdef RASCSI
		// Command reception handshake (10 bytes are automatically received at the first command)
		count = ctrl.bus->CommandHandShake(ctrl.buffer);

		// If no byte can be received move to the status phase
		if (count == 0) {
			Error();
			return;
		}

		// Check 10-byte CDB
		if (ctrl.buffer[0] >= 0x20 && ctrl.buffer[0] <= 0x7D) {
			ctrl.length = 10;
		}

		// If not able to receive all, move to the status phase
		if (count != (int)ctrl.length) {
			Error();
			return;
		}

		// Command data transfer
		for (i = 0; i < (int)ctrl.length; i++) {
			ctrl.cmd[i] = (DWORD)ctrl.buffer[i];
		}

		// Clear length and block
		ctrl.length = 0;
		ctrl.blocks = 0;

		// Execution Phase
		Execute();
#else
		// Request the command
		ctrl.bus->SetREQ(TRUE);
		return;
#endif	// RASCSI
	}
#ifndef RASCSI
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
//	Execution Phase
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::Execute()
{
	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "Execution Phase Command %02X", ctrl.cmd[0]);
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

		// REZERO UNIT
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

		// FORMAT UNIT
		case 0x06:
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

		// ASSIGN(SASIのみ)
		case 0x0e:
			CmdAssign();
			return;

		// SPECIFY(SASIのみ)
		case 0xc2:
			CmdSpecify();
			return;
	}

	// Unsupported command
	Log(Log::Warning, "Unsupported command $%02X", ctrl.cmd[0]);
	CmdInvalid();
}

//---------------------------------------------------------------------------
//
//	Status phase
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::Status()
{
#ifdef RASCSI
	DWORD min_exec_time;
	DWORD time;
#endif	// RASCSI

	ASSERT(this);

	// Phase change
	if (ctrl.phase != BUS::status) {

#ifdef RASCSI
		// Minimum execution time
		if (ctrl.execstart > 0) {
			min_exec_time = IsSASI() ? min_exec_time_sasi : min_exec_time_scsi;
			time = SysTimer::GetTimerLow() - ctrl.execstart;
			if (time < min_exec_time) {
				SysTimer::SleepUsec(min_exec_time - time);
			}
			ctrl.execstart = 0;
		} else {
			SysTimer::SleepUsec(5);
		}
#endif	// RASCSI

#if defined(DISK_LOG)
		Log(Log::Normal, "Status phase");
#endif	// DISK_LOG

		// Phase Setting
		ctrl.phase = BUS::status;

		// Signal line operated by the target
		ctrl.bus->SetMSG(FALSE);
		ctrl.bus->SetCD(TRUE);
		ctrl.bus->SetIO(TRUE);

		// Data transfer is 1 byte x 1 block
		ctrl.offset = 0;
		ctrl.length = 1;
		ctrl.blocks = 1;
		ctrl.buffer[0] = (BYTE)ctrl.status;

#ifndef RASCSI
		// Request status
		ctrl.bus->SetDAT(ctrl.buffer[0]);
		ctrl.bus->SetREQ(TRUE);

#if defined(DISK_LOG)
		Log(Log::Normal, "Status Phase $%02X", ctrl.status);
#endif	// DISK_LOG
#endif	// RASCSI
		return;
	}

#ifdef RASCSI
	// Send
	Send();
#else
	// Requesting
	if (ctrl.bus->GetREQ()) {
		// Initiator received
		if (ctrl.bus->GetACK()) {
			SendNext();
		}
	} else {
		// Initiator requests next
		if (!ctrl.bus->GetACK()) {
			Send();
		}
	}
#endif	// RASCSI
}

//---------------------------------------------------------------------------
//
//	Message in phase
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::MsgIn()
{
	ASSERT(this);

	// Phase change
	if (ctrl.phase != BUS::msgin) {

#if defined(DISK_LOG)
		Log(Log::Normal, "Message in phase");
#endif	// DISK_LOG

		// Phase Setting
		ctrl.phase = BUS::msgin;

		// Signal line operated by the target
		ctrl.bus->SetMSG(TRUE);
		ctrl.bus->SetCD(TRUE);
		ctrl.bus->SetIO(TRUE);

		// length, blocks are already set
		ASSERT(ctrl.length > 0);
		ASSERT(ctrl.blocks > 0);
		ctrl.offset = 0;

#ifndef RASCSI
		// Request message
		ctrl.bus->SetDAT(ctrl.buffer[ctrl.offset]);
		ctrl.bus->SetREQ(TRUE);

#if defined(DISK_LOG)
		Log(Log::Normal, "Message in phase $%02X", ctrl.buffer[ctrl.offset]);
#endif	// DISK_LOG
#endif	// RASCSI
		return;
	}

#ifdef RASCSI
	//Send
	Send();
#else
	// Requesting
	if (ctrl.bus->GetREQ()) {
		// Initator received
		if (ctrl.bus->GetACK()) {
			SendNext();
		}
	} else {
		// Initiator requests next
		if (!ctrl.bus->GetACK()) {
			Send();
		}
	}
#endif	// RASCSI
}

//---------------------------------------------------------------------------
//
//	Data-in Phase
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::DataIn()
{
#ifdef RASCSI
	DWORD min_exec_time;
	DWORD time;
#endif	// RASCSI

	ASSERT(this);
	ASSERT(ctrl.length >= 0);

	// Phase change
	if (ctrl.phase != BUS::datain) {

#ifdef RASCSI
		// Minimum execution time
		if (ctrl.execstart > 0) {
			min_exec_time = IsSASI() ? min_exec_time_sasi : min_exec_time_scsi;
			time = SysTimer::GetTimerLow() - ctrl.execstart;
			if (time < min_exec_time) {
				SysTimer::SleepUsec(min_exec_time - time);
			}
			ctrl.execstart = 0;
		}
#endif	// RASCSI

		// If the length is 0, go to the status phase
		if (ctrl.length == 0) {
			Status();
			return;
		}

#if defined(DISK_LOG)
		Log(Log::Normal, "Data-in Phase");
#endif	// DISK_LOG

		// Phase Setting
		ctrl.phase = BUS::datain;

		// Signal line operated by the target
		ctrl.bus->SetMSG(FALSE);
		ctrl.bus->SetCD(FALSE);
		ctrl.bus->SetIO(TRUE);

		// length, blocks are already set
		ASSERT(ctrl.length > 0);
		ASSERT(ctrl.blocks > 0);
		ctrl.offset = 0;

#ifndef RASCSI
		// Assert the DAT signal
		ctrl.bus->SetDAT(ctrl.buffer[ctrl.offset]);

		// Request data
		ctrl.bus->SetREQ(TRUE);
#endif	// RASCSI
		return;
	}

#ifdef RASCSI
	// Send
	Send();
#else
	// Requesting
	if (ctrl.bus->GetREQ()) {
		// Initator received
		if (ctrl.bus->GetACK()) {
			SendNext();
		}
	} else {
		// Initiator requests next
		if (!ctrl.bus->GetACK()) {
			Send();
		}
	}
#endif	// RASCSI
}

//---------------------------------------------------------------------------
//
//	Data out phase
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::DataOut()
{
#ifdef RASCSI
	DWORD min_exec_time;
	DWORD time;
#endif	// RASCSI

	ASSERT(this);
	ASSERT(ctrl.length >= 0);

	// Phase change
	if (ctrl.phase != BUS::dataout) {

#ifdef RASCSI
		// Minimum execution time
		if (ctrl.execstart > 0) {
			min_exec_time = IsSASI() ? min_exec_time_sasi : min_exec_time_scsi;
			time = SysTimer::GetTimerLow() - ctrl.execstart;
			if (time < min_exec_time) {
				SysTimer::SleepUsec(min_exec_time - time);
			}
			ctrl.execstart = 0;
		}
#endif	// RASCSI

		// If the length is 0, go to the status phase
		if (ctrl.length == 0) {
			Status();
			return;
		}

#if defined(DISK_LOG)
		Log(Log::Normal, "Data out phase");
#endif	// DISK_LOG

		// Phase Setting
		ctrl.phase = BUS::dataout;

		// Signal line operated by the target
		ctrl.bus->SetMSG(FALSE);
		ctrl.bus->SetCD(FALSE);
		ctrl.bus->SetIO(FALSE);

		// length, blocks are already calculated
		ASSERT(ctrl.length > 0);
		ASSERT(ctrl.blocks > 0);
		ctrl.offset = 0;

#ifndef	RASCSI
		// Request data
		ctrl.bus->SetREQ(TRUE);
#endif	// RASCSI
		return;
	}

#ifdef	RASCSI
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
//	Error
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::Error()
{
	DWORD lun;

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
	Log(Log::Warning, "Error occured (going to status phase)");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;

	// Set status and message(CHECK CONDITION)
	ctrl.status = (lun << 5) | 0x02;

	// status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	TEST UNIT READY
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::CmdTestUnitReady()
{
	DWORD lun;
	BOOL status;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "TEST UNIT READY Command ");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	status = ctrl.unit[lun]->TestUnitReady(ctrl.cmd);
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
//	REZERO UNIT
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::CmdRezero()
{
	DWORD lun;
	BOOL status;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "REZERO UNIT Command ");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	status = ctrl.unit[lun]->Rezero(ctrl.cmd);
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
//	REQUEST SENSE
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::CmdRequestSense()
{
	DWORD lun;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "REQUEST SENSE Command ");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	ctrl.length = ctrl.unit[lun]->RequestSense(ctrl.cmd, ctrl.buffer);
	ASSERT(ctrl.length > 0);

#if defined(DISK_LOG)
	Log(Log::Normal, "Sense key $%02X", ctrl.buffer[2]);
#endif	// DISK_LOG

	// Read phase
	DataIn();
}

//---------------------------------------------------------------------------
//
//	FORMAT UNIT
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::CmdFormat()
{
	DWORD lun;
	BOOL status;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "FORMAT UNIT Command ");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	status = ctrl.unit[lun]->Format(ctrl.cmd);
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
//	REASSIGN BLOCKS
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::CmdReassign()
{
	DWORD lun;
	BOOL status;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "REASSIGN BLOCKS Command ");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	status = ctrl.unit[lun]->Reassign(ctrl.cmd);
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
//	READ(6)
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::CmdRead6()
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

	// Get record number and block number
	record = ctrl.cmd[1] & 0x1f;
	record <<= 8;
	record |= ctrl.cmd[2];
	record <<= 8;
	record |= ctrl.cmd[3];
	ctrl.blocks = ctrl.cmd[4];
	if (ctrl.blocks == 0) {
		ctrl.blocks = 0x100;
	}

#if defined(DISK_LOG)
	Log(Log::Normal,
		"READ(6) command record=%06X blocks=%d", record, ctrl.blocks);
#endif	// DISK_LOG

	// Command processing on drive
	ctrl.length = ctrl.unit[lun]->Read(ctrl.buffer, record);
	if (ctrl.length <= 0) {
		// Failure (Error)
		Error();
		return;
	}

	// Set next block
	ctrl.next = record + 1;

	// Read phase
	DataIn();
}

//---------------------------------------------------------------------------
//
//	WRITE(6)
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::CmdWrite6()
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

	// Get record number and block number
	record = ctrl.cmd[1] & 0x1f;
	record <<= 8;
	record |= ctrl.cmd[2];
	record <<= 8;
	record |= ctrl.cmd[3];
	ctrl.blocks = ctrl.cmd[4];
	if (ctrl.blocks == 0) {
		ctrl.blocks = 0x100;
	}

#if defined(DISK_LOG)
	Log(Log::Normal,
		"WRITE(6) command record=%06X blocks=%d", record, ctrl.blocks);
#endif	// DISK_LOG

	// Command processing on drive
	ctrl.length = ctrl.unit[lun]->WriteCheck(record);
	if (ctrl.length <= 0) {
		// Failure (Error)
		Error();
		return;
	}

	// Set next block
	ctrl.next = record + 1;

	// Write phase
	DataOut();
}

//---------------------------------------------------------------------------
//
//	SEEK(6)
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::CmdSeek6()
{
	DWORD lun;
	BOOL status;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "SEEK(6) Command ");
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
//	ASSIGN
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::CmdAssign()
{
	DWORD lun;
	BOOL status;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "ASSIGN Command ");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	status = ctrl.unit[lun]->Assign(ctrl.cmd);
	if (!status) {
		// Failure (Error)
		Error();
		return;
	}

	// Request 4 bytes of data
	ctrl.length = 4;

	// Write phase
	DataOut();
}

//---------------------------------------------------------------------------
//
//	SPECIFY
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::CmdSpecify()
{
	DWORD lun;
	BOOL status;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "SPECIFY Command ");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	status = ctrl.unit[lun]->Assign(ctrl.cmd);
	if (!status) {
		// Failure (Error)
		Error();
		return;
	}

	// Request 10 bytes of data
	ctrl.length = 10;

	// Write phase
	DataOut();
}

//---------------------------------------------------------------------------
//
//	Unsupported command
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::CmdInvalid()
{
	DWORD lun;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "Command not supported");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (ctrl.unit[lun]) {
		// Command processing on drive
		ctrl.unit[lun]->InvalidCmd();
	}

	// Failure (Error)
	Error();
}

//===========================================================================
//
//	Data transfer
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Data transmission
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::Send()
{
#ifdef RASCSI
	int len;
#endif	// RASCSI
	BOOL result;

	ASSERT(this);
	ASSERT(!ctrl.bus->GetREQ());
	ASSERT(ctrl.bus->GetIO());

#ifdef RASCSI
	// Check that the length isn't 0
	if (ctrl.length != 0) {
		len = ctrl.bus->SendHandShake(
			&ctrl.buffer[ctrl.offset], ctrl.length);

		// If you can not send it all, move on to the status phase
		if (len != (int)ctrl.length) {
			Error();
			return;
		}

		// Offset and Length
		ctrl.offset += ctrl.length;
		ctrl.length = 0;
		return;
	}
#else
	// Offset and Length
	ASSERT(ctrl.length >= 1);
	ctrl.offset++;
	ctrl.length--;

	// Immediately after ACK is asserted, if the data
	// has been set by SendNext, raise the request
	if (ctrl.length != 0) {
		// Signal line operated by the target
		ctrl.bus->SetREQ(TRUE);
		return;
	}
#endif	// RASCSI

	// Remove block and initialize the result
	ctrl.blocks--;
	result = TRUE;

	// Process after data collection (read/data-in only)
	if (ctrl.phase == BUS::datain) {
		if (ctrl.blocks != 0) {
			// Set next buffer (set offset, length)
			result = XferIn(ctrl.buffer);
			//** printf("xfer in: %d \n",result);

#ifndef RASCSI
			ctrl.bus->SetDAT(ctrl.buffer[ctrl.offset]);
#endif	// RASCSI
		}
	}

	// If result FALSE, move to the status phase
	if (!result) {
		Error();
		return;
	}

	// Continue sending if block != 0
	if (ctrl.blocks != 0){
		ASSERT(ctrl.length > 0);
		ASSERT(ctrl.offset == 0);
#ifndef RASCSI
		// Signal line operated by the target
		ctrl.bus->SetREQ(TRUE);
#endif	// RASCSI
		return;
	}

	// Move to the next phase
	switch (ctrl.phase) {
		// Message in phase
		case BUS::msgin:
			// Bus free phase
			BusFree();
			break;

		// Data-in Phase
		case BUS::datain:
			// status phase
			Status();
			break;

		// Status phase
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

#ifndef	RASCSI
//---------------------------------------------------------------------------
//
//	Continue sending data
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::SendNext()
{
	ASSERT(this);

	// Req is up
	ASSERT(ctrl.bus->GetREQ());
	ASSERT(ctrl.bus->GetIO());

	// Signal line operated by the target
	ctrl.bus->SetREQ(FALSE);

	// If there is data in the buffer, set it first.
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
void FASTCALL SASIDEV::Receive()
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
				if (ctrl.cmd[0] >= 0x20 && ctrl.cmd[0] <= 0x7D) {
					// 10 byte CDB
					ctrl.length = 10;
				}
			}
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
//	Receive data
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::Receive()
#else
//---------------------------------------------------------------------------
//
//	Continue receiving data
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::ReceiveNext()
#endif	// RASCSI
{
#ifdef RASCSI
	int len;
#endif	// RASCSI
	BOOL result;

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
		ctrl.length = 0;
		return;
	}
#else
	// Offset and Length
	ASSERT(ctrl.length >= 1);
	ctrl.offset++;
	ctrl.length--;

	// If length != 0, set req again
	if (ctrl.length != 0) {
		// Signal line operated by the target
		ctrl.bus->SetREQ(TRUE);
		return;
	}
#endif	// RASCSI

	// Remove the control block and initialize the result
	ctrl.blocks--;
	result = TRUE;

	// Process the data out phase
	if (ctrl.phase == BUS::dataout) {
		if (ctrl.blocks == 0) {
			// End with this buffer
			result = XferOut(FALSE);
		} else {
			// Continue to next buffer (set offset, length)
			result = XferOut(TRUE);
		}
	}

	// If result is false, move to the status phase
	if (!result) {
		Error();
		return;
	}

	// Continue to receive is block != 0
	if (ctrl.blocks != 0){
		ASSERT(ctrl.length > 0);
		ASSERT(ctrl.offset == 0);
#ifndef RASCSI
		// Signal line operated by the target
		ctrl.bus->SetREQ(TRUE);
#endif	// RASCSI
		return;
	}

	// Move to the next phase
	switch (ctrl.phase) {
#ifndef RASCSI
		// Command phase
		case BUS::command:
			// Execution Phase
			Execute();
			break;
#endif	// RASCSI

		// Data out phase
		case BUS::dataout:
			// Flush
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
//	Data transfer IN
//	*Reset offset and length
//
//---------------------------------------------------------------------------
BOOL FASTCALL SASIDEV::XferIn(BYTE *buf)
{
	DWORD lun;

	ASSERT(this);
	ASSERT(ctrl.phase == BUS::datain);

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		return FALSE;
	}

	// Limited to read commands
	switch (ctrl.cmd[0]) {
		// READ(6)
		case 0x08:
		// READ(10)
		case 0x28:
			// Read from disk
			ctrl.length = ctrl.unit[lun]->Read(buf, ctrl.next);
			ctrl.next++;

			// If there is an error, go to the status phase
			if (ctrl.length <= 0) {
				// Cancel data-in
				return FALSE;
			}

			// If things are normal, work setting
			ctrl.offset = 0;
			break;

		// Other (impossible)
		default:
			ASSERT(FALSE);
			return FALSE;
	}

	// Succeeded in setting the buffer
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Data transfer OUT
//	*If cont=true, reset the offset and length
//
//---------------------------------------------------------------------------
BOOL FASTCALL SASIDEV::XferOut(BOOL cont)
{
	DWORD lun;
	SCSIBR *bridge;

	ASSERT(this);
	ASSERT(ctrl.phase == BUS::dataout);

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		return FALSE;
	}

	// MODE SELECT or WRITE system
	switch (ctrl.cmd[0]) {
		// MODE SELECT
		case 0x15:
		// MODE SELECT(10)
		case 0x55:
			if (!ctrl.unit[lun]->ModeSelect(
				ctrl.cmd, ctrl.buffer, ctrl.offset)) {
				// MODE SELECT failed
				return FALSE;
			}
			break;

		// WRITE(6)
		case 0x0a:
		// WRITE(10)
		case 0x2a:
			// Replace the host bridge with SEND MESSAGE 10
			if (ctrl.unit[lun]->GetID() == MAKEID('S', 'C', 'B', 'R')) {
				bridge = (SCSIBR*)ctrl.unit[lun];
				if (!bridge->SendMessage10(ctrl.cmd, ctrl.buffer)) {
					// write failed
					return FALSE;
				}

				// If normal, work setting
				ctrl.offset = 0;
				break;
			}

		// WRITE AND VERIFY
		case 0x2e:
			// Write
			if (!ctrl.unit[lun]->Write(ctrl.buffer, ctrl.next - 1)) {
				// Write failed
				return FALSE;
			}

			// If you do not need the next block, end here
			ctrl.next++;
			if (!cont) {
				break;
			}

			// Check the next block
			ctrl.length = ctrl.unit[lun]->WriteCheck(ctrl.next - 1);
			if (ctrl.length <= 0) {
				// Cannot write
				return FALSE;
			}

			// If normal, work setting
			ctrl.offset = 0;
			break;

		// SPECIFY(SASI only)
		case 0xc2:
			break;

		default:
			ASSERT(FALSE);
			break;
	}

	// Buffer saved successfully
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Logical unit flush
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::FlushUnit()
{
	DWORD lun;

	ASSERT(this);
	ASSERT(ctrl.phase == BUS::dataout);

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		return;
	}

	// WRITE system only
	switch (ctrl.cmd[0]) {
		// WRITE(6)
		case 0x0a:
		// WRITE(10)
		case 0x2a:
		// WRITE AND VERIFY
		case 0x2e:
			// Flush
			if (!ctrl.unit[lun]->IsCacheWB()) {
				ctrl.unit[lun]->Flush();
			}
			break;
        // Mode Select (6)
        case 0x15:
        // MODE SELECT(10)
		case 0x55:
            // Debug code related to Issue #2 on github, where we get an unhandled Model select when
            // the mac is rebooted
            // https://github.com/akuker/RASCSI/issues/2
            Log(Log::Warning, "Received \'Mode Select\'\n");
            Log(Log::Warning, "   Operation Code: [%02X]\n", ctrl.cmd[0]);
            Log(Log::Warning, "   Logical Unit %01X, PF %01X, SP %01X [%02X]\n", ctrl.cmd[1] >> 5, 1 & (ctrl.cmd[1] >> 4), ctrl.cmd[1] & 1, ctrl.cmd[1]);
            Log(Log::Warning, "   Reserved: %02X\n", ctrl.cmd[2]);
            Log(Log::Warning, "   Reserved: %02X\n", ctrl.cmd[3]);
            Log(Log::Warning, "   Parameter List Len %02X\n", ctrl.cmd[4]);
            Log(Log::Warning, "   Reserved: %02X\n", ctrl.cmd[5]);
            Log(Log::Warning, "   Ctrl Len: %08X\n",ctrl.length);

			if (!ctrl.unit[lun]->ModeSelect(
				ctrl.cmd, ctrl.buffer, ctrl.offset)) {
				// MODE SELECT failed
				Log(Log::Warning, "Error occured while processing Mode Select command %02X\n", (unsigned char)ctrl.cmd[0]);
				return;
			}
            break;

		default:
			Log(Log::Warning, "Received an invalid flush command %02X!!!!!\n",ctrl.cmd[0]);
			ASSERT(FALSE);
			break;
	}
}

#ifdef DISK_LOG
//---------------------------------------------------------------------------
//
//	Get the current phase as a string
//
//---------------------------------------------------------------------------
void SASIDEV::GetPhaseStr(char *str)
{
    switch(this->GetPhase())
    {
        case BUS::busfree:
        strcpy(str,"busfree    ");
        break;
        case BUS::arbitration:
        strcpy(str,"arbitration");
        break;
        case BUS::selection:
        strcpy(str,"selection  ");
        break;
        case BUS::reselection:
        strcpy(str,"reselection");
        break;
        case BUS::command:
        strcpy(str,"command    ");
        break;
        case BUS::execute:
        strcpy(str,"execute    ");
        break;
        case BUS::datain:
        strcpy(str,"datain     ");
        break;
        case BUS::dataout:
        strcpy(str,"dataout    ");
        break;
        case BUS::status:
        strcpy(str,"status     ");
        break;
        case BUS::msgin:
        strcpy(str,"msgin      ");
        break;
        case BUS::msgout:
        strcpy(str,"msgout     ");
        break;
        case BUS::reserved:
        strcpy(str,"reserved   ");
        break;
    }
}
#endif

//---------------------------------------------------------------------------
//
//	Log output
//
// TODO: This function needs some cleanup. Its very kludgey
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::Log(Log::loglevel level, const char *format, ...)
{
#if !defined(BAREMETAL)
#ifdef DISK_LOG
	char buffer[0x200];
	char buffer2[0x250];
	char buffer3[0x250];
	char phase_str[20];
#endif
	va_list args;
	va_start(args, format);

	if(this->GetID() != 6)
	{
        return;
	}

#ifdef RASCSI
#ifndef DISK_LOG
	if (level == Log::Warning) {
		return;
	}
#endif	// DISK_LOG
#endif	// RASCSI

#ifdef DISK_LOG
	// format
	vsprintf(buffer, format, args);

	// end variable length argument
	va_end(args);

	// Add the date/timestamp
	// current date/time based on current system
   time_t now = time(0);
   // convert now to string form
   char* dt = ctime(&now);


   strcpy(buffer2, "[");
   strcat(buffer2, dt);
   // Get rid of the carriage return
   buffer2[strlen(buffer2)-1] = '\0';
   strcat(buffer2, "] ");

   // Get the phase
   this->GetPhaseStr(phase_str);
   sprintf(buffer3, "[%d][%s] ", this->GetID(), phase_str);
   strcat(buffer2,buffer3);
   strcat(buffer2, buffer);


	// Log output
#ifdef RASCSI
	printf("%s\n", buffer2);
#else
	host->GetVM()->GetLog()->Format(level, host, buffer);
#endif	// RASCSI
#endif	// BAREMETAL
#endif	// DISK_LOG
}

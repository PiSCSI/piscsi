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
//	 See LICENSE file in the project root folder.
//
//	[ SASI device controller ]
//
//---------------------------------------------------------------------------
#include "controllers/sasidev_ctrl.h"
#include "filepath.h"
#include "gpiobus.h"
#include "devices/scsi_host_bridge.h"
#include "controllers/scsidev_ctrl.h"
#include "devices/scsi_daynaport.h"
#include "exceptions.h"

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
	ctrl.m_scsi_id = UNKNOWN_SCSI_ID;
	ctrl.bus = NULL;
	memset(ctrl.cmd, 0x00, sizeof(ctrl.cmd));
	ctrl.status = 0x00;
	ctrl.message = 0x00;
	ctrl.execstart = 0;
	// The initial buffer size will default to either the default buffer size OR 
	// the size of an Ethernet message, whichever is larger.
	ctrl.bufsize = std::max(DEFAULT_BUFFER_SIZE, ETH_FRAME_LEN + 16 + ETH_FCS_LEN);
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

	ctrl.m_scsi_id = id;
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
	if (ctrl.m_scsi_id < 0 || ctrl.bus == NULL) {
		return ctrl.phase;
	}

	// Get bus information
	((GPIOBUS*)ctrl.bus)->Aquire();

	// For the monitor tool, we shouldn't need to reset. We're just logging information
	// Reset
	if (ctrl.bus->GetRST()) {
		LOGINFO("RESET signal received");

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

		LOGINFO("Bus free phase");

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
		id = 1 << ctrl.m_scsi_id;
		if ((ctrl.bus->GetDAT() & id) == 0) {
			return;
		}

		// Return if there is no unit
		if (!HasUnit()) {
			return;
		}

		LOGTRACE("%s Selection Phase ID=%d (with device)", __PRETTY_FUNCTION__, (int)ctrl.m_scsi_id);

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

		LOGTRACE("%s Command Phase", __PRETTY_FUNCTION__);

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
                try {
                        Execute();
                }
                catch (lunexception& e) {
                        LOGINFO("%s unsupported LUN %d", __PRETTY_FUNCTION__, (int)e.getlun());
                        Error();
                }
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

	LOGTRACE("%s Execution Phase Command %02X", __PRETTY_FUNCTION__, (WORD)ctrl.cmd[0]);

	// Phase Setting
	ctrl.phase = BUS::execute;

	// Initialization for data transfer
	ctrl.offset = 0;
	ctrl.blocks = 1;
	#ifdef RASCSI
	ctrl.execstart = SysTimer::GetTimerLow();
	#endif	// RASCSI

	// Process by command
	switch ((SCSIDEV::scsi_command)ctrl.cmd[0]) {
		// TEST UNIT READY
		case SCSIDEV::eCmdTestUnitReady:
			CmdTestUnitReady();
			return;

		// REZERO UNIT
		case SCSIDEV::eCmdRezero:
			CmdRezero();
			return;

		// REQUEST SENSE
		case SCSIDEV::eCmdRequestSense:
			CmdRequestSense();
			return;

		// FORMAT UNIT
		// This doesn't exist in the SCSI Spec, but was in the original RaSCSI code.
		// leaving it here for now....
		case SCSIDEV::eCmdFormat:
			CmdFormat();
			return;

		// REASSIGN BLOCKS
		case SCSIDEV::eCmdReassign:
			CmdReassign();
			return;

		// READ(6)
		case SCSIDEV::eCmdRead6:
			CmdRead6();
			return;

		// WRITE(6)
		case SCSIDEV::eCmdWrite6:
			CmdWrite6();
			return;

		// SEEK(6)
		case SCSIDEV::eCmdSeek6:
			CmdSeek6();
			return;

		// ASSIGN(SASIのみ)
		// This doesn't exist in the SCSI Spec, but was in the original RaSCSI code.
		// leaving it here for now....
		case SCSIDEV::eCmdSasiCmdAssign:
			CmdAssign();
			return;

		// RESERVE UNIT(16)
		case 0x16:
			CmdReserveUnit();
			return;
		
		// RELEASE UNIT(17)
		case 0x17:
			CmdReleaseUnit();
			return;

		// SPECIFY(SASIのみ)
		// This doesn't exist in the SCSI Spec, but was in the original RaSCSI code.
		// leaving it here for now....
		case SCSIDEV::eCmdInvalid:
			CmdSpecify();
			return;
		default:
			break;
	}

	// Unsupported command
	LOGWARN("%s Unsupported command $%02X", __PRETTY_FUNCTION__, (WORD)ctrl.cmd[0]);
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

		LOGTRACE("%s Status phase", __PRETTY_FUNCTION__);

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

		LOGTRACE( "%s Status Phase $%02X",__PRETTY_FUNCTION__, (unsigned int)ctrl.status);
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
		LOGTRACE("%s Starting Message in phase", __PRETTY_FUNCTION__);

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
		return;
	}

	//Send
	LOGTRACE("%s Transitioning to Send()", __PRETTY_FUNCTION__);
	Send();
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

		LOGTRACE("%s Going into Data-in Phase", __PRETTY_FUNCTION__);
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

		return;
	}

	// Send
	Send();
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

		LOGTRACE("%s Data out phase", __PRETTY_FUNCTION__);

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

#if defined(DISK_LOG)
	LOGWARN("Error occured (going to status phase)");
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
	BOOL status;

	ASSERT(this);

	LOGTRACE("%s TEST UNIT READY Command ", __PRETTY_FUNCTION__);

        DWORD lun = GetLun();

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
	BOOL status;

	ASSERT(this);

	LOGTRACE( "%s REZERO UNIT Command ", __PRETTY_FUNCTION__);

        DWORD lun = GetLun();

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
	ASSERT(this);

	LOGTRACE( "%s REQUEST SENSE Command ", __PRETTY_FUNCTION__);

        DWORD lun = GetLun();

	ctrl.length = ctrl.unit[lun]->RequestSense(ctrl.cmd, ctrl.buffer);
	ASSERT(ctrl.length > 0);


	LOGTRACE("%s Sense key $%02X",__PRETTY_FUNCTION__, (WORD)ctrl.buffer[2]);

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
	BOOL status;

	ASSERT(this);

	LOGTRACE( "%s FORMAT UNIT Command ", __PRETTY_FUNCTION__);

        DWORD lun = GetLun();

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
	BOOL status;

	ASSERT(this);

	LOGTRACE("%s REASSIGN BLOCKS Command ", __PRETTY_FUNCTION__);

        DWORD lun = GetLun();

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
//	RESERVE UNIT(16)
//
//  The reserve/release commands are only used in multi-initiator
//  environments. RaSCSI doesn't support this use case. However, some old
//  versions of Solaris will issue the reserve/release commands. We will
//  just respond with an OK status.
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::CmdReserveUnit()
{
	ASSERT(this);
	LOGTRACE( "%s Reserve(6) Command", __PRETTY_FUNCTION__);

	// status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	RELEASE UNIT(17)
//
//  The reserve/release commands are only used in multi-initiator
//  environments. RaSCSI doesn't support this use case. However, some old
//  versions of Solaris will issue the reserve/release commands. We will
//  just respond with an OK status.
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::CmdReleaseUnit()
{
	ASSERT(this);
	LOGTRACE( "%s Release(6) Command", __PRETTY_FUNCTION__);

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
	DWORD record;

	ASSERT(this);

        DWORD lun = GetLun();

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

	if(ctrl.unit[lun]->GetID() == MAKEID('S', 'C', 'D', 'P')){
		// The DaynaPort only wants one block.
		// ctrl.cmd[4] and ctrl.cmd[5] are used to specify the maximum buffer size for the DaynaPort
		ctrl.blocks=1;
	}

	LOGTRACE("%s READ(6) command record=%06X blocks=%d ID %08X", __PRETTY_FUNCTION__, (unsigned int)record, (int)ctrl.blocks, (unsigned int)ctrl.unit[lun]->GetID());

	// Command processing on drive
	ctrl.length = ctrl.unit[lun]->Read(ctrl.cmd, ctrl.buffer, record);
	LOGTRACE("%s ctrl.length is %d", __PRETTY_FUNCTION__, (int)ctrl.length);

	// The DaynaPort will respond a status of 0x02 when a read of size 1 occurs.
	if ((ctrl.length <= 0) && (ctrl.unit[lun]->GetID() != MAKEID('S', 'C', 'D', 'P'))) {
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
//  This Send Message command is used by the DaynaPort SCSI/Link
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::DaynaPortWrite()
{
	DWORD data_format;

	ASSERT(this);

        DWORD lun = GetLun();

	// Error if not a host bridge
	if (ctrl.unit[lun]->GetID() != MAKEID('S', 'C', 'D', 'P')) {
		LOGERROR("Received DaynaPortWrite for a non-DaynaPort device");
		Error();
		return;
	}

	// Reallocate buffer (because it is not transfer for each block)
	if (ctrl.bufsize < DAYNAPORT_BUFFER_SIZE) {
		free(ctrl.buffer);
		ctrl.bufsize = DAYNAPORT_BUFFER_SIZE;
		ctrl.buffer = (BYTE *)malloc(ctrl.bufsize);
	}


	data_format = ctrl.cmd[5];


	if(data_format == 0x00){
		ctrl.length = (WORD)ctrl.cmd[4] + ((WORD)ctrl.cmd[3] << 8);
	}
	else if (data_format == 0x80){
		ctrl.length = (WORD)ctrl.cmd[4] + ((WORD)ctrl.cmd[3] << 8) + 8;
	}
	else
	{
		LOGWARN("%s Unknown data format %02X", __PRETTY_FUNCTION__, (unsigned int)data_format);
	}
	LOGTRACE("%s length: %04X (%d) format: %02X", __PRETTY_FUNCTION__, (unsigned int)ctrl.length, (int)ctrl.length, (unsigned int)data_format);

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
//	WRITE(6)
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::CmdWrite6()
{
	DWORD record;

	ASSERT(this);

        DWORD lun = GetLun();

	// Special receive function for the DaynaPort
	if (ctrl.unit[lun]->GetID() == MAKEID('S', 'C', 'D', 'P')){
		DaynaPortWrite();
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

	LOGTRACE("%s WRITE(6) command record=%06X blocks=%d", __PRETTY_FUNCTION__, (WORD)record, (WORD)ctrl.blocks);

	// Command processing on drive
	ctrl.length = ctrl.unit[lun]->WriteCheck(record);
	if (ctrl.length <= 0) {
		LOGDEBUG("WriteCheck failed");
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
	BOOL status;

	ASSERT(this);

	LOGTRACE("%s SEEK(6) Command ", __PRETTY_FUNCTION__);

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
//	ASSIGN
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::CmdAssign()
{
	BOOL status;

	ASSERT(this);

	LOGTRACE("%s ASSIGN Command ", __PRETTY_FUNCTION__);

        DWORD lun = GetLun();

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
	BOOL status;

	ASSERT(this);

	LOGTRACE("%s SPECIFY Command ", __PRETTY_FUNCTION__);

        DWORD lun = GetLun();

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
	ASSERT(this);
	LOGWARN("%s Command not supported", __PRETTY_FUNCTION__);

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

	// Check that the length isn't 0
	if (ctrl.length != 0) {
		len = ctrl.bus->SendHandShake(
			&ctrl.buffer[ctrl.offset], ctrl.length, BUS::SEND_NO_DELAY);

		// If you can not send it all, move on to the status phase
		if (len != (int)ctrl.length) {
			LOGERROR("%s ctrl.length (%d) did not match the amount of data sent (%d)",__PRETTY_FUNCTION__, (int)ctrl.length, len);
			Error();
			return;
		}

		// Offset and Length
		ctrl.offset += ctrl.length;
		ctrl.length = 0;
		return;
	}
	else{
		LOGINFO("%s ctrl.length was 0", __PRETTY_FUNCTION__);
	}

	// Remove block and initialize the result
	ctrl.blocks--;
	result = TRUE;

	// Process after data collection (read/data-in only)
	if (ctrl.phase == BUS::datain) {
		if (ctrl.blocks != 0) {
			// Set next buffer (set offset, length)
			result = XferIn(ctrl.buffer);
			LOGTRACE("%s xfer in: %d",__PRETTY_FUNCTION__, result);
			LOGTRACE("%s processing after data collection", __PRETTY_FUNCTION__);
#ifndef RASCSI
			ctrl.bus->SetDAT(ctrl.buffer[ctrl.offset]);
			#endif	// RASCSI
		}
	}

	// If result FALSE, move to the status phase
	if (!result) {
		LOGERROR("%s Send result was false", __PRETTY_FUNCTION__);
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
			LOGTRACE( "%s Command phase $%02X", __PRETTY_FUNCTION__, data);

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
		LOGDEBUG("%s Received %d bytes", __PRETTY_FUNCTION__, len);

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
	else
	{
		LOGDEBUG("%s ctrl.length was 0", __PRETTY_FUNCTION__);
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
			LOGTRACE("%s transitioning to FlushUnit()",__PRETTY_FUNCTION__);
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
	LOGTRACE("%s ctrl.cmd[0]=%02X", __PRETTY_FUNCTION__, (unsigned int)ctrl.cmd[0]);

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
			ctrl.length = ctrl.unit[lun]->Read(ctrl.cmd, buf, ctrl.next);
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

	switch ((SCSIDEV::scsi_command) ctrl.cmd[0]) {
		case SCSIDEV::eCmdModeSelect:
		case SCSIDEV::eCmdModeSelect10:
			if (!ctrl.unit[lun]->ModeSelect(
				ctrl.cmd, ctrl.buffer, ctrl.offset)) {
				// MODE SELECT failed
				return FALSE;
			}
			break;

		case SCSIDEV::eCmdWrite6:
		case SCSIDEV::eCmdWrite10:
		case SCSIDEV::eCmdWriteAndVerify10:
			// If we're a host bridge, use the host bridge's SendMessage10 
			// function
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

			// Special case Write function for DaynaPort
			if (ctrl.unit[lun]->GetID() == MAKEID('S', 'C', 'D', 'P')) {
				LOGTRACE("%s Doing special case write for DaynaPort", __PRETTY_FUNCTION__);
				if (!(SCSIDaynaPort*)ctrl.unit[lun]->Write(ctrl.cmd, ctrl.buffer, ctrl.length)) {
					// write failed
					return FALSE;
				}
				LOGTRACE("%s Done with DaynaPort Write", __PRETTY_FUNCTION__);

				// If normal, work setting
				ctrl.offset = 0;
				ctrl.blocks = 0;
				break;
			}

			LOGTRACE("%s eCmdWriteAndVerify10 Calling Write... cmd: %02X next: %d", __PRETTY_FUNCTION__, (unsigned int)ctrl.cmd, (int)ctrl.next);
			if (!ctrl.unit[lun]->Write(ctrl.cmd, ctrl.buffer, ctrl.next - 1)) {
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
		case SCSIDEV::eCmdInvalid:
			break;

		case SCSIDEV::eCmdSetMcastAddr:
			LOGTRACE("%s Done with DaynaPort Set Multicast Address", __PRETTY_FUNCTION__);
			break;
		default:
			LOGWARN("Received an unexpected command (%02X) in %s", (WORD)ctrl.cmd[0] , __PRETTY_FUNCTION__)
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
	switch ((SCSIDEV::scsi_command)ctrl.cmd[0]) {
		case SCSIDEV::eCmdWrite6:
		case SCSIDEV::eCmdWrite10:
		case SCSIDEV::eCmdWriteAndVerify10:
			// Flush
			if (!ctrl.unit[lun]->IsCacheWB()) {
				ctrl.unit[lun]->Flush();
			}
			break;
		case SCSIDEV::eCmdModeSelect:
		case SCSIDEV::eCmdModeSelect10:
            // Debug code related to Issue #2 on github, where we get an unhandled Model select when
            // the mac is rebooted
            // https://github.com/akuker/RASCSI/issues/2
            LOGWARN("Received \'Mode Select\'\n");
            LOGWARN("   Operation Code: [%02X]\n", (WORD)ctrl.cmd[0]);
            LOGWARN("   Logical Unit %01X, PF %01X, SP %01X [%02X]\n",\
			   (WORD)ctrl.cmd[1] >> 5, 1 & ((WORD)ctrl.cmd[1] >> 4), \
			   (WORD)ctrl.cmd[1] & 1, (WORD)ctrl.cmd[1]);
            LOGWARN("   Reserved: %02X\n", (WORD)ctrl.cmd[2]);
            LOGWARN("   Reserved: %02X\n", (WORD)ctrl.cmd[3]);
            LOGWARN("   Parameter List Len %02X\n", (WORD)ctrl.cmd[4]);
            LOGWARN("   Reserved: %02X\n",(WORD)ctrl.cmd[5]);
            LOGWARN("   Ctrl Len: %08X\n",(WORD)ctrl.length);

			if (!ctrl.unit[lun]->ModeSelect(
				ctrl.cmd, ctrl.buffer, ctrl.offset)) {
				// MODE SELECT failed
				LOGWARN("Error occured while processing Mode Select command %02X\n", (unsigned char)ctrl.cmd[0]);
				return;
			}
            break;
		case SCSIDEV::eCmdSetIfaceMode:
			LOGWARN("%s Trying to flush a command set interface mode. This should be a daynaport?", __PRETTY_FUNCTION__);
			break;
		case SCSIDEV::eCmdSetMcastAddr:
			// TODO: Eventually, we should store off the multicast address configuration data here...
			break;
		default:
			LOGWARN("Received an unexpected flush command %02X!!!!!\n",(WORD)ctrl.cmd[0]);
			// The following statement makes debugging a huge pain. You can un-comment it
			// if you're not trying to add new devices....
			// ASSERT(FALSE);
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
//	Validate LUN
//
//---------------------------------------------------------------------------
DWORD FASTCALL SASIDEV::GetLun()
{
        DWORD lun = (ctrl.cmd[1] >> 5) & 0x07;
        if (!ctrl.unit[lun]) {
                throw lunexception(lun);
        }

        return lun;
}

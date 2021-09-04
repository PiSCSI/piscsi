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
#include "devices/scsi_daynaport.h"
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
	scsi.atnmsg = false;
	scsi.msc = 0;
	memset(scsi.msb, 0x00, sizeof(scsi.msb));
}

SCSIDEV::~SCSIDEV()
{
}

//---------------------------------------------------------------------------
//
//	Device reset
//
//---------------------------------------------------------------------------
void SCSIDEV::Reset()
{
	// Work initialization
	scsi.atnmsg = false;
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
		scsi.atnmsg = false;
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

	// Discard pending sense data from the previous command if the current command is not REQUEST SENSE
	if ((SCSIDEV::scsi_command)ctrl.cmd[0] != eCmdRequestSense) {
		ctrl.status = 0;
	}

	LOGDEBUG("++++ CMD ++++ %s Executing command $%02X", __PRETTY_FUNCTION__, (unsigned int)ctrl.cmd[0]);

	// Use LUN0 for INQUIRY and REQUEST SENSE because LUN0 is assumed to be always available.
	// INQUIRY and REQUEST SENSE have a special LUN handling of their own, required by the SCSI standard.
	int lun = 0;
	if ((SCSIDEV::scsi_command)ctrl.cmd[0] != eCmdInquiry && (SCSIDEV::scsi_command)ctrl.cmd[0] != eCmdRequestSense) {
		lun = (ctrl.cmd[1] >> 5) & 0x07;

		if (!ctrl.unit[lun]) {
			LOGINFO("Invalid LUN %d for ID %d", lun, GetSCSIID());

			Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::INVALID_LUN);
			return;
		}
	}

	ctrl.device = ctrl.unit[lun];

	if (!ctrl.device->Dispatch(this)) {
		LOGWARN("ID %d LUN %d received unsupported command: $%02X", GetSCSIID(), lun, (BYTE)ctrl.cmd[0]);

		Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::INVALID_COMMAND_OPERATION_CODE);
	}

	if ((SCSIDEV::scsi_command)ctrl.cmd[0] == eCmdInquiry) {
		// SCSI-2 p.104 4.4.3 Incorrect logical unit handling
		if (((ctrl.cmd[1] >> 5) & 0x07) != (DWORD)ctrl.device->GetLun()) {
			ctrl.buffer[0] = 0x7f;
		}
	}
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
			scsi.atnmsg = true;
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

	DWORD lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun] || asc == ERROR_CODES::INVALID_LUN) {
		lun = 0;
	}

	if (sense_key || asc) {
		// Set Sense Key and ASC for a subsequent REQUEST SENSE
		ctrl.unit[lun]->SetStatusCode((sense_key << 16) | (asc << 8));
	}

	ctrl.status = 0x02;
	ctrl.message = 0x00;

	LOGTRACE("%s Error (to status phase)", __PRETTY_FUNCTION__);

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

	ASSERT(!ctrl.bus->GetREQ());
	ASSERT(ctrl.bus->GetIO());

	//if Length! = 0, send
	if (ctrl.length != 0) {
		ostringstream s;
		s << __PRETTY_FUNCTION__ << " sending handhake with offset " << ctrl.offset << ", length " << ctrl.length;
		LOGTRACE("%s", s.str().c_str());

		// TODO Get rid of Daynaport specific code
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
	bool result = true;

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
				scsi.atnmsg = false;

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
	bool result = true;

	// Processing after receiving data (by phase)
	LOGTRACE("%s ctrl.phase: %d (%s)",__PRETTY_FUNCTION__, (int)ctrl.phase, BUS::GetPhaseStrRaw(ctrl.phase));
	switch (ctrl.phase) {

		// Data out phase
		case BUS::dataout:
			if (ctrl.blocks == 0) {
				// End with this buffer
				result = XferOut(false);
			} else {
				// Continue to next buffer (set offset, length)
				result = XferOut(true);
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
			scsi.atnmsg = false;

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
bool SCSIDEV::XferMsg(DWORD msg)
{
	ASSERT(ctrl.phase == BUS::msgout);

	// Save message out data
	if (scsi.atnmsg) {
		scsi.msb[scsi.msc] = (BYTE)msg;
		scsi.msc++;
		scsi.msc %= 256;
	}

	return true;
}


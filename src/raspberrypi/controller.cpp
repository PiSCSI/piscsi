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
#include "controller.h"
#include "gpiobus.h"
#include "exceptions.h"
#include "devices/scsi_host_bridge.h"
#include "devices/scsi_daynaport.h"

using namespace scsi_defs;

Controller::Controller()
{
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
	ctrl.lun = -1;

	// Logical unit initialization
	for (int i = 0; i < UNIT_MAX; i++) {
		ctrl.unit[i] = NULL;
	}

	scsi.is_byte_transfer = false;
	scsi.bytes_to_transfer = 0;
	shutdown_mode = NONE;

	// Synchronous transfer work initialization
	scsi.syncenable = FALSE;
	scsi.syncperiod = 50;
	scsi.syncoffset = 0;
	scsi.atnmsg = false;
	scsi.msc = 0;
	memset(scsi.msb, 0x00, sizeof(scsi.msb));
}

Controller::~Controller()
{
	if (ctrl.buffer) {
		free(ctrl.buffer);
		ctrl.buffer = NULL;
	}
}

void Controller::Reset()
{
	memset(ctrl.cmd, 0x00, sizeof(ctrl.cmd));
	ctrl.phase = BUS::busfree;
	ctrl.status = 0x00;
	ctrl.message = 0x00;
	ctrl.execstart = 0;
	memset(ctrl.buffer, 0x00, ctrl.bufsize);
	ctrl.blocks = 0;
	ctrl.next = 0;
	ctrl.offset = 0;
	ctrl.length = 0;

	scsi.atnmsg = false;
	scsi.msc = 0;
	scsi.is_byte_transfer = false;
	scsi.bytes_to_transfer = 0;
	memset(scsi.msb, 0x00, sizeof(scsi.msb));

	for (int i = 0; i < UNIT_MAX; i++) {
		if (ctrl.unit[i]) {
			ctrl.unit[i]->Reset();
		}
	}
}

void Controller::SetUnit(int no, PrimaryDevice *dev)
{
	assert(no < UNIT_MAX);

	ctrl.unit[no] = dev;
}

bool Controller::HasUnit() const
{
	for (int i = 0; i < UNIT_MAX; i++) {
		if (ctrl.unit[i]) {
			return true;
		}
	}

	return false;
}

void Controller::Connect(int id, BUS *bus)
{
	ctrl.m_scsi_id = id;
	ctrl.bus = bus;
}

BUS::phase_t Controller::Process(int initiator_id)
{
	// Do nothing if not connected
	if (ctrl.m_scsi_id < 0 || ctrl.bus == NULL) {
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

	scsi.initiator_id = initiator_id;

	// Phase processing
	switch (ctrl.phase) {
		// Bus free phase
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

		// Message out (MCI=110)
		case BUS::msgout:
			MsgOut();
			break;

		// Message in (MCI=111)
		case BUS::msgin:
			MsgIn();
			break;

		default:
			assert(false);
			break;
	}

	return ctrl.phase;
}

void Controller::BusFree()
{
	// Phase change
	if (ctrl.phase != BUS::busfree) {
		LOGTRACE("%s Bus free phase", __PRETTY_FUNCTION__);

		// Phase setting
		ctrl.phase = BUS::busfree;

		// Set Signal lines
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

		ctrl.lun = -1;

		scsi.is_byte_transfer = false;
		scsi.bytes_to_transfer = 0;

		// When the bus is free RaSCSI or the Pi may be shut down
		switch(shutdown_mode) {
		case STOP_RASCSI:
			LOGINFO("RaSCSI shutdown requested");
			exit(0);
			break;

		case STOP_PI:
			LOGINFO("Raspberry Pi shutdown requested");
			if (system("init 0") == -1) {
				LOGERROR("Raspberry Pi shutdown failed: %s", strerror(errno));
			}
			break;

		case RESTART_PI:
			LOGINFO("Raspberry Pi restart requested");
			if (system("init 6") == -1) {
				LOGERROR("Raspberry Pi restart failed: %s", strerror(errno));
			}
			break;

		default:
			break;
		}

		return;
	}

	// Move to selection phase
	if (ctrl.bus->GetSEL() && !ctrl.bus->GetBSY()) {
		Selection();
	}
}

void Controller::Selection()
{
	// Phase change
	if (ctrl.phase != BUS::selection) {
		// invalid if IDs do not match
		int id = 1 << ctrl.m_scsi_id;
		if ((ctrl.bus->GetDAT() & id) == 0) {
			return;
		}

		// Return if there is no valid LUN
		if (!HasUnit()) {
			return;
		}

		LOGTRACE("%s Selection Phase ID=%d (with device)", __PRETTY_FUNCTION__, (int)ctrl.m_scsi_id);

		if (scsi.initiator_id != UNKNOWN_SCSI_ID) {
			LOGTRACE("%s Initiator ID is %d", __PRETTY_FUNCTION__, scsi.initiator_id);
		}
		else {
			LOGTRACE("%s Initiator ID is unknown", __PRETTY_FUNCTION__);
		}

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

void Controller::Command()
{
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

		// If no byte can be received move to the status phase
		int count = ctrl.bus->CommandHandShake(ctrl.buffer);
		if (!count) {
			LOGERROR("%s No byte received. Going to error",__PRETTY_FUNCTION__);
			Error();
			return;
		}

		ctrl.length = GPIOBUS::GetCommandByteCount(ctrl.buffer[0]);

		// If not able to receive all, move to the status phase
		if (count != (int)ctrl.length) {
			Error();
			return;
		}

		// Command data transfer
		for (int i = 0; i < (int)ctrl.length; i++) {
			ctrl.cmd[i] = ctrl.buffer[i];
			LOGTRACE("%s CDB[%d]=$%02X",__PRETTY_FUNCTION__, i, ctrl.cmd[i]);
		}

		// Clear length and block
		ctrl.length = 0;
		ctrl.blocks = 0;

		Execute();
	}
}

void Controller::Execute()
{
	LOGTRACE("%s Execution phase command $%02X", __PRETTY_FUNCTION__, (unsigned int)ctrl.cmd[0]);

	// Phase Setting
	ctrl.phase = BUS::execute;

	// Initialization for data transfer
	ctrl.offset = 0;
	ctrl.blocks = 1;
	ctrl.execstart = SysTimer::GetTimerLow();

	// Discard pending sense data from the previous command if the current command is not REQUEST SENSE
	if ((scsi_command)ctrl.cmd[0] != scsi_command::eCmdRequestSense) {
		ctrl.status = 0;
	}

	LOGDEBUG("++++ CMD ++++ %s Executing command $%02X", __PRETTY_FUNCTION__, (unsigned int)ctrl.cmd[0]);

	int lun = GetEffectiveLun();
	if (!ctrl.unit[lun]) {
		if ((scsi_command)ctrl.cmd[0] != scsi_command::eCmdInquiry &&
				(scsi_command)ctrl.cmd[0] != scsi_command::eCmdRequestSense) {
			LOGDEBUG("Invalid LUN %d for ID %d", lun, GetSCSIID());

			Error(sense_key::ILLEGAL_REQUEST, asc::INVALID_LUN);
			return;
		}
		// Use LUN 0 for INQUIRY and REQUEST SENSE because LUN0 is assumed to be always available.
		// INQUIRY and REQUEST SENSE have a special LUN handling of their own, required by the SCSI standard.
		else {
			assert(ctrl.unit[0]);

			lun = 0;
		}
	}

	ctrl.device = ctrl.unit[lun];

	// Discard pending sense data from the previous command if the current command is not REQUEST SENSE
	if ((scsi_command)ctrl.cmd[0] != scsi_command::eCmdRequestSense) {
		ctrl.device->SetStatusCode(0);
	}
	
	try {
		if (!ctrl.device->Dispatch(this)) {
			LOGTRACE("ID %d LUN %d received unsupported command: $%02X", GetSCSIID(), lun, (BYTE)ctrl.cmd[0]);

			throw scsi_dispatch_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_COMMAND_OPERATION_CODE);
		}
	}
	catch(const scsi_dispatch_error_exception& e) {
		Error(e.get_sense_key(), e.get_asc(), e.get_status());

		// Fall through
	}

	// SCSI-2 p.104 4.4.3 Incorrect logical unit handling
	if ((scsi_command)ctrl.cmd[0] == scsi_command::eCmdInquiry && !ctrl.unit[lun]) {
		lun = GetEffectiveLun();

		LOGTRACE("Reporting LUN %d for device ID %d as not supported", lun, ctrl.device->GetId());

		ctrl.buffer[0] = 0x7f;
	}
}

void Controller::Status()
{
	// Phase change
	if (ctrl.phase != BUS::status) {
		// Minimum execution time
		if (ctrl.execstart > 0) {
			uint32_t time = SysTimer::GetTimerLow() - ctrl.execstart;
			if (time < MIN_EXEC_TIME) {
				SysTimer::SleepUsec(MIN_EXEC_TIME - time);
			}
			ctrl.execstart = 0;
		} else {
			SysTimer::SleepUsec(5);
		}

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

		LOGTRACE( "%s Status Phase $%02X",__PRETTY_FUNCTION__, (unsigned int)ctrl.status);

		return;
	}

	Send();
}

void Controller::MsgIn()
{
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
		assert(ctrl.length > 0);
		assert(ctrl.blocks > 0);
		ctrl.offset = 0;
		return;
	}

	LOGTRACE("%s Transitioning to Send()", __PRETTY_FUNCTION__);
	Send();
}

void Controller::MsgOut()
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

	Receive();
}

void Controller::DataIn()
{
	assert(ctrl.length >= 0);

	// Phase change
	if (ctrl.phase != BUS::datain) {
		// Minimum execution time
		if (ctrl.execstart > 0) {
			uint32_t time = SysTimer::GetTimerLow() - ctrl.execstart;
			if (time < MIN_EXEC_TIME) {
				SysTimer::SleepUsec(MIN_EXEC_TIME - time);
			}
			ctrl.execstart = 0;
		}

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
		assert(ctrl.length > 0);
		assert(ctrl.blocks > 0);
		ctrl.offset = 0;

		return;
	}

	Send();
}

void Controller::DataOut()
{
	assert(ctrl.length >= 0);

	// Phase change
	if (ctrl.phase != BUS::dataout) {
		// Minimum execution time
		if (ctrl.execstart > 0) {
			uint32_t time = SysTimer::GetTimerLow() - ctrl.execstart;
			if (time < MIN_EXEC_TIME) {
				SysTimer::SleepUsec(MIN_EXEC_TIME - time);
			}
			ctrl.execstart = 0;
		}

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

		// Length has already been calculated
		assert(ctrl.length > 0);
		ctrl.offset = 0;
		return;
	}

	Receive();
}

void Controller::Error(sense_key sense_key, asc asc, status status)
{
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

	int lun = GetEffectiveLun();
	if (!ctrl.unit[lun] || asc == INVALID_LUN) {
		lun = 0;

		assert(ctrl.unit[lun]);
	}

	if (sense_key || asc) {
		// Set Sense Key and ASC for a subsequent REQUEST SENSE
		ctrl.unit[lun]->SetStatusCode((sense_key << 16) | (asc << 8));
	}

	ctrl.status = status;
	ctrl.message = 0x00;

	LOGTRACE("%s Error (to status phase)", __PRETTY_FUNCTION__);

	Status();
}

void Controller::Send()
{
	assert(!ctrl.bus->GetREQ());
	assert(ctrl.bus->GetIO());

	if (ctrl.length != 0) {
		LOGTRACE("%s%s", __PRETTY_FUNCTION__, (" Sending handhake with offset " + to_string(ctrl.offset) + ", length "
				+ to_string(ctrl.length)).c_str());

		// TODO The delay has to be taken from ctrl.unit[lun], but as there are no Daynaport drivers for
		// LUNs other than 0 this work-around works.
		int len = ctrl.bus->SendHandShake(&ctrl.buffer[ctrl.offset], ctrl.length, ctrl.unit[0] ? ctrl.unit[0]->GetSendDelay() : 0);

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
			LOGTRACE("%s%s", __PRETTY_FUNCTION__, (" Processing after data collection. Blocks: " + to_string(ctrl.blocks)).c_str());
		}
	}

	// If result FALSE, move to status phase
	if (!result) {
		Error();
		return;
	}

	// Continue sending if block !=0
	if (ctrl.blocks != 0){
		LOGTRACE("%s%s", __PRETTY_FUNCTION__, (" Continuing to send. Blocks: " + to_string(ctrl.blocks)).c_str());
		assert(ctrl.length > 0);
		assert(ctrl.offset == 0);
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

		default:
			assert(false);
			break;
	}
}

void Controller::Receive()
{
	if (scsi.is_byte_transfer) {
		ReceiveBytes();
		return;
	}

	int len;
	BYTE data;

	LOGTRACE("%s",__PRETTY_FUNCTION__);

	// REQ is low
	assert(!ctrl.bus->GetREQ());
	assert(!ctrl.bus->GetIO());

	// Length != 0 if received
	if (ctrl.length != 0) {
		LOGTRACE("%s Length is %d bytes", __PRETTY_FUNCTION__, (int)ctrl.length);
		// Receive
		len = ctrl.bus->ReceiveHandShake(&ctrl.buffer[ctrl.offset], ctrl.length);

		// If not able to receive all, move to status phase
		if (len != (int)ctrl.length) {
			LOGERROR("%s Not able to receive %d bytes of data, only received %d. Going to error",__PRETTY_FUNCTION__, (int)ctrl.length, len);
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
		assert(ctrl.length > 0);
		assert(ctrl.offset == 0);
		return;
	}

	// Move to next phase
	switch (ctrl.phase) {
		case BUS::command:
			len = GPIOBUS::GetCommandByteCount(ctrl.buffer[0]);

			for (int i = 0; i < len; i++) {
				ctrl.cmd[i] = ctrl.buffer[i];
				LOGTRACE("%s Command Byte %d: $%02X",__PRETTY_FUNCTION__, i, ctrl.cmd[i]);
			}

			Execute();
			break;

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
						LOGTRACE("Message code ABORT $%02X", data);
						BusFree();
						return;
					}

					// BUS DEVICE RESET
					if (data == 0x0C) {
						LOGTRACE("Message code BUS DEVICE RESET $%02X", data);
						scsi.syncoffset = 0;
						BusFree();
						return;
					}

					// IDENTIFY
					if (data >= 0x80) {
						ctrl.lun = data & 0x1F;
						LOGTRACE("Message code IDENTIFY $%02X, LUN %d selected", data, ctrl.lun);
					}

					// Extended Message
					if (data == 0x01) {
						LOGTRACE("Message code EXTENDED MESSAGE $%02X", data);

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
							scsi.syncperiod = 50;
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

			Command();
			break;

		case BUS::dataout:
			FlushUnit();

			Status();
			break;

		default:
			assert(false);
			break;
	}
}

bool Controller::XferMsg(int msg)
{
	assert(ctrl.phase == BUS::msgout);

	// Save message out data
	if (scsi.atnmsg) {
		scsi.msb[scsi.msc] = (BYTE)msg;
		scsi.msc++;
		scsi.msc %= 256;
	}

	return true;
}

void Controller::ReceiveBytes()
{
	uint32_t len;
	BYTE data;

	LOGTRACE("%s",__PRETTY_FUNCTION__);

	// REQ is low
	assert(!ctrl.bus->GetREQ());
	assert(!ctrl.bus->GetIO());

	if (ctrl.length) {
		LOGTRACE("%s Length is %d bytes", __PRETTY_FUNCTION__, ctrl.length);

		len = ctrl.bus->ReceiveHandShake(&ctrl.buffer[ctrl.offset], ctrl.length);

		// If not able to receive all, move to status phase
		if (len != ctrl.length) {
			LOGERROR("%s Not able to receive %d bytes of data, only received %d. Going to error",
					__PRETTY_FUNCTION__, ctrl.length, len);
			Error();
			return;
		}

		ctrl.offset += ctrl.length;
		scsi.bytes_to_transfer = ctrl.length;
		ctrl.length = 0;
		return;
	}

	// Result initialization
	bool result = true;

	// Processing after receiving data (by phase)
	LOGTRACE("%s ctrl.phase: %d (%s)",__PRETTY_FUNCTION__, (int)ctrl.phase, BUS::GetPhaseStrRaw(ctrl.phase));
	switch (ctrl.phase) {

		case BUS::dataout:
			result = XferOut(false);
			break;

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

	// Move to next phase
	switch (ctrl.phase) {
		case BUS::command:
			len = GPIOBUS::GetCommandByteCount(ctrl.buffer[0]);

			for (uint32_t i = 0; i < len; i++) {
				ctrl.cmd[i] = ctrl.buffer[i];
				LOGTRACE("%s Command Byte %d: $%02X",__PRETTY_FUNCTION__, i, ctrl.cmd[i]);
			}

			Execute();
			break;

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
						LOGTRACE("Message code ABORT $%02X", data);
						BusFree();
						return;
					}

					// BUS DEVICE RESET
					if (data == 0x0C) {
						LOGTRACE("Message code BUS DEVICE RESET $%02X", data);
						scsi.syncoffset = 0;
						BusFree();
						return;
					}

					// IDENTIFY
					if (data >= 0x80) {
						ctrl.lun = data & 0x1F;
						LOGTRACE("Message code IDENTIFY $%02X, LUN %d selected", data, ctrl.lun);
					}

					// Extended Message
					if (data == 0x01) {
						LOGTRACE("Message code EXTENDED MESSAGE $%02X", data);

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

			Command();
			break;

		case BUS::dataout:
			Status();
			break;

		default:
			assert(false);
			break;
	}
}

bool Controller::XferOut(bool cont)
{
	if (!scsi.is_byte_transfer) {
		return XferOutBlockOriented(cont);
	}

	assert(ctrl.phase == BUS::dataout);

	scsi.is_byte_transfer = false;

	PrimaryDevice *device = ctrl.unit[GetEffectiveLun()];
	if (device && ctrl.cmd[0] == scsi_command::eCmdWrite6) {
		return device->WriteBytes(ctrl.buffer, scsi.bytes_to_transfer);
	}

	LOGWARN("Received an unexpected command ($%02X) in %s", (WORD)ctrl.cmd[0] , __PRETTY_FUNCTION__)

	return false;
}

void Controller::FlushUnit()
{
	assert(ctrl.phase == BUS::dataout);

	int lun = GetEffectiveLun();
	if (!ctrl.unit[lun]) {
		return;
	}

	Disk *disk = (Disk *)ctrl.unit[lun];

	// WRITE system only
	switch ((Controller::rw_command)ctrl.cmd[0]) {
		case Controller::eCmdWrite6:
		case Controller::eCmdWrite10:
		case Controller::eCmdWrite16:
		case Controller::eCmdWriteLong10:
		case Controller::eCmdWriteLong16:
		case Controller::eCmdVerify10:
		case Controller::eCmdVerify16:
			break;

		case Controller::eCmdModeSelect6:
		case Controller::eCmdModeSelect10:
            // Debug code related to Issue #2 on github, where we get an unhandled Mode Select when
            // the mac is rebooted
            // https://github.com/akuker/RASCSI/issues/2
			// TODO Verfiy whether this ticket is still valid
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

			if (!disk->ModeSelect(ctrl.cmd, ctrl.buffer, ctrl.offset)) {
				// MODE SELECT failed
				LOGWARN("Error occured while processing Mode Select command %02X\n", (unsigned char)ctrl.cmd[0]);
				return;
			}
            break;

		case Controller::eCmdSetMcastAddr:
			// TODO: Eventually, we should store off the multicast address configuration data here...
			break;

		default:
			LOGWARN("Received an unexpected flush command $%02X\n",(WORD)ctrl.cmd[0]);
			break;
	}
}

//---------------------------------------------------------------------------
//
//	Data transfer IN
//	*Reset offset and length
// TODO XferIn probably needs a dispatcher, in order to avoid subclassing Disk, i.e.
// just like the actual SCSI commands XferIn should be executed by the respective device
//
//---------------------------------------------------------------------------
bool Controller::XferIn(BYTE *buf)
{
	assert(ctrl.phase == BUS::datain);
	LOGTRACE("%s ctrl.cmd[0]=%02X", __PRETTY_FUNCTION__, (unsigned int)ctrl.cmd[0]);

	int lun = GetEffectiveLun();
	if (!ctrl.unit[lun]) {
		return false;
	}

	// Limited to read commands
	switch (ctrl.cmd[0]) {
		case eCmdRead6:
		case eCmdRead10:
		case eCmdRead16:
			// Read from disk
			ctrl.length = ((Disk *)ctrl.unit[lun])->Read(ctrl.cmd, buf, ctrl.next);
			ctrl.next++;

			// If there is an error, go to the status phase
			if (ctrl.length <= 0) {
				// Cancel data-in
				return false;
			}

			// If things are normal, work setting
			ctrl.offset = 0;
			break;

		// Other (impossible)
		default:
			assert(false);
			return false;
	}

	// Succeeded in setting the buffer
	return true;
}

//---------------------------------------------------------------------------
//
//	Data transfer OUT
//	*If cont=true, reset the offset and length
// TODO XferOut probably needs a dispatcher, in order to avoid subclassing Disk, i.e.
// just like the actual SCSI commands XferOut should be executed by the respective device
//
//---------------------------------------------------------------------------
bool Controller::XferOutBlockOriented(bool cont)
{
	assert(ctrl.phase == BUS::dataout);

	int lun = GetEffectiveLun();
	if (!ctrl.unit[lun]) {
		return false;
	}

	Disk *device = (Disk *)ctrl.unit[lun];

	// Limited to write commands
	switch (ctrl.cmd[0]) {
		case eCmdModeSelect6:
		case eCmdModeSelect10:
			if (!device->ModeSelect(ctrl.cmd, ctrl.buffer, ctrl.offset)) {
				// MODE SELECT failed
				return false;
			}
			break;

		case eCmdWrite6:
		case eCmdWrite10:
		case eCmdWrite16:
		case eCmdVerify10:
		case eCmdVerify16:
		{
			// If we're a host bridge, use the host bridge's SendMessage10 function
			// TODO This class must not know about SCSIBR
			SCSIBR *bridge = dynamic_cast<SCSIBR *>(device);
			if (bridge) {
				if (!bridge->SendMessage10(ctrl.cmd, ctrl.buffer)) {
					// write failed
					return false;
				}

				// If normal, work setting
				ctrl.offset = 0;
				break;
			}

			// Special case Write function for DaynaPort
			// TODO This class must not know about DaynaPort
			SCSIDaynaPort *daynaport = dynamic_cast<SCSIDaynaPort *>(device);
			if (daynaport) {
				if (!daynaport->Write(ctrl.cmd, ctrl.buffer, ctrl.length)) {
					// write failed
					return false;
				}

				// If normal, work setting
				ctrl.offset = 0;
				ctrl.blocks = 0;
				break;
			}

			if (!device->Write(ctrl.cmd, ctrl.buffer, ctrl.next - 1)) {
				// Write failed
				return false;
			}

			// If you do not need the next block, end here
			ctrl.next++;
			if (!cont) {
				break;
			}

			// Check the next block
			ctrl.length = device->WriteCheck(ctrl.next - 1);
			if (ctrl.length <= 0) {
				// Cannot write
				return false;
			}

			// If normal, work setting
			ctrl.offset = 0;
			break;
		}

		case eCmdSetMcastAddr:
			LOGTRACE("%s Done with DaynaPort Set Multicast Address", __PRETTY_FUNCTION__);
			break;

		default:
			LOGWARN("Received an unexpected command ($%02X) in %s", (WORD)ctrl.cmd[0] , __PRETTY_FUNCTION__)
			break;
	}

	return true;
}

int Controller::GetEffectiveLun() const
{
	return ctrl.lun != -1 ? ctrl.lun : (ctrl.cmd[1] >> 5) & 0x07;
}


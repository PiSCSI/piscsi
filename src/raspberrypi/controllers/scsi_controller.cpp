//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//  	Copyright (C) akuker
//
//  	Licensed under the BSD 3-Clause License.
//  	See LICENSE file in the project root folder.
//
//---------------------------------------------------------------------------

#include "log.h"
#include "hal/gpiobus.h"
#include "hal/systimer.h"
#include "rascsi_exceptions.h"
#include "devices/scsi_host_bridge.h"
#include "devices/scsi_daynaport.h"
#include "scsi_controller.h"
#include <sstream>
#include <iomanip>
#ifdef __linux__
#include <linux/if_tun.h>
#endif

using namespace scsi_defs;

ScsiController::ScsiController(BUS& bus, int target_id) : AbstractController(bus, target_id, LUN_MAX)
{
	// The initial buffer size will default to either the default buffer size OR
	// the size of an Ethernet message, whichever is larger.
	AllocateBuffer(std::max(DEFAULT_BUFFER_SIZE, ETH_FRAME_LEN + 16 + ETH_FCS_LEN));
}

void ScsiController::Reset()
{
	AbstractController::Reset();

	execstart = 0;
	identified_lun = -1;

	scsi.atnmsg = false;
	scsi.msc = 0;
	scsi.msb = {};

	is_byte_transfer = false;
	bytes_to_transfer = 0;
}

BUS::phase_t ScsiController::Process(int id)
{
	// Get bus information
	bus.Acquire();

	// Check to see if the reset signal was asserted
	if (bus.GetRST()) {
		LOGWARN("RESET signal received!")

		// Reset the controller
		Reset();

		// Reset the bus
		bus.Reset();

		return GetPhase();
	}

	if (id != UNKNOWN_INITIATOR_ID) {
		LOGTRACE("%s Initiator ID is %d", __PRETTY_FUNCTION__, id)
	}
	else {
		LOGTRACE("%s Initiator ID is unknown", __PRETTY_FUNCTION__)
	}

	initiator_id = id;

	try {
		ProcessPhase();
	}
	catch(const scsi_exception&) {
		// Any exception should have been handled during the phase processing
		assert(false);

		LOGERROR("%s Unhandled SCSI error, resetting controller and bus and entering bus free phase", __PRETTY_FUNCTION__)

		Reset();
		bus.Reset();

		BusFree();
	}

	return GetPhase();
}

void ScsiController::BusFree()
{
	if (!IsBusFree()) {
		LOGTRACE("%s Bus free phase", __PRETTY_FUNCTION__)

		SetPhase(BUS::phase_t::busfree);

		bus.SetREQ(false);
		bus.SetMSG(false);
		bus.SetCD(false);
		bus.SetIO(false);
		bus.SetBSY(false);

		// Initialize status and message
		SetStatus(status::GOOD);
		ctrl.message = 0x00;

		// Initialize ATN message reception status
		scsi.atnmsg = false;

		identified_lun = -1;

		is_byte_transfer = false;
		bytes_to_transfer = 0;

		// When the bus is free RaSCSI or the Pi may be shut down.
		// This code has to be executed in the bus free phase and thus has to be located in the controller.
		switch(shutdown_mode) {
		case rascsi_shutdown_mode::STOP_RASCSI:
			LOGINFO("RaSCSI shutdown requested")
			exit(0);
			break;

		case rascsi_shutdown_mode::STOP_PI:
			LOGINFO("Raspberry Pi shutdown requested")
			if (system("init 0") == -1) {
				LOGERROR("Raspberry Pi shutdown failed: %s", strerror(errno))
			}
			break;

		case rascsi_shutdown_mode::RESTART_PI:
			LOGINFO("Raspberry Pi restart requested")
			if (system("init 6") == -1) {
				LOGERROR("Raspberry Pi restart failed: %s", strerror(errno))
			}
			break;

		default:
			break;
		}

		return;
	}

	// Move to selection phase
	if (bus.GetSEL() && !bus.GetBSY()) {
		Selection();
	}
}

void ScsiController::Selection()
{
	if (!IsSelection()) {
		// A different device controller was selected
		if (int id = 1 << GetTargetId(); ((int)bus.GetDAT() & id) == 0) {
			return;
		}

		// Abort if there is no LUN for this controller
		if (!GetLunCount()) {
			return;
		}

		LOGTRACE("%s Selection Phase Target ID=%d", __PRETTY_FUNCTION__, GetTargetId())

		SetPhase(BUS::phase_t::selection);

		// Raise BSY and respond
		bus.SetBSY(true);
		return;
	}

	// Selection completed
	if (!bus.GetSEL() && bus.GetBSY()) {
		// Message out phase if ATN=1, otherwise command phase
		if (bus.GetATN()) {
			MsgOut();
		} else {
			Command();
		}
	}
}

void ScsiController::Command()
{
	if (!IsCommand()) {
		LOGTRACE("%s Command Phase", __PRETTY_FUNCTION__)

		SetPhase(BUS::phase_t::command);

		bus.SetMSG(false);
		bus.SetCD(true);
		bus.SetIO(false);

		const int actual_count = bus.CommandHandShake(GetBuffer().data());
		const int command_byte_count = GPIOBUS::GetCommandByteCount(GetBuffer()[0]);

		// If not able to receive all, move to the status phase
		if (actual_count != command_byte_count) {
			LOGERROR("%s Command byte count mismatch: expected %d bytes, received %d byte(s)", __PRETTY_FUNCTION__,
					command_byte_count, actual_count)
			Error(sense_key::ABORTED_COMMAND);
			return;
		}

		InitCmd(command_byte_count);

		// Command data transfer
		stringstream s;
		for (int i = 0; i < command_byte_count; i++) {
			ctrl.cmd[i] = GetBuffer()[i];
			s << setfill('0') << setw(2) << hex << ctrl.cmd[i];
		}
		LOGTRACE("%s CDB=$%s",__PRETTY_FUNCTION__, s.str().c_str())

		ctrl.length = 0;

		Execute();
	}
}

void ScsiController::Execute()
{
	LOGDEBUG("++++ CMD ++++ %s Executing command $%02X", __PRETTY_FUNCTION__, (int)GetOpcode())

	// Initialization for data transfer
	ResetOffset();
	ctrl.blocks = 1;
	execstart = SysTimer::instance().GetTimerLow();

	// Discard pending sense data from the previous command if the current command is not REQUEST SENSE
	if (GetOpcode() != scsi_command::eCmdRequestSense) {
		SetStatus(status::GOOD);
	}

	int lun = GetEffectiveLun();
	if (!HasDeviceForLun(lun)) {
		if (GetOpcode() != scsi_command::eCmdInquiry && GetOpcode() != scsi_command::eCmdRequestSense) {
			LOGDEBUG("Invalid LUN %d for ID %d", lun, GetTargetId())

			Error(sense_key::ILLEGAL_REQUEST, asc::INVALID_LUN);
			return;
		}
		// Use LUN 0 for INQUIRY and REQUEST SENSE because LUN0 is assumed to be always available.
		// INQUIRY and REQUEST SENSE have a special LUN handling of their own, required by the SCSI standard.
		else {
			assert(HasDeviceForLun(0));

			lun = 0;
		}
	}

	auto device = GetDeviceForLun(lun);

	// Discard pending sense data from the previous command if the current command is not REQUEST SENSE
	if (GetOpcode() != scsi_command::eCmdRequestSense) {
		device->SetStatusCode(0);
	}

	try {
		if (!device->Dispatch(GetOpcode())) {
			LOGTRACE("ID %d LUN %d received unsupported command: $%02X", GetTargetId(), lun, (int)GetOpcode())

			throw scsi_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_COMMAND_OPERATION_CODE);
		}
	}
	catch(const scsi_exception& e) { //NOSONAR This exception is handled properly
		Error(e.get_sense_key(), e.get_asc(), e.get_status());

		// Fall through
	}

	// SCSI-2 p.104 4.4.3 Incorrect logical unit handling
	if (GetOpcode() == scsi_command::eCmdInquiry && !HasDeviceForLun(lun)) {
		lun = GetEffectiveLun();

		LOGTRACE("Reporting LUN %d for device ID %d as not supported", lun, device->GetId())

		GetBuffer().data()[0] = 0x7f;
	}
}

void ScsiController::Status()
{
	if (!IsStatus()) {
		// Minimum execution time
		if (execstart > 0) {
			Sleep();
		} else {
			SysTimer::instance().SleepUsec(5);
		}

		LOGTRACE("%s Status Phase, status is $%02X",__PRETTY_FUNCTION__, (int)GetStatus())

		SetPhase(BUS::phase_t::status);

		// Signal line operated by the target
		bus.SetMSG(false);
		bus.SetCD(true);
		bus.SetIO(true);

		// Data transfer is 1 byte x 1 block
		ResetOffset();
		ctrl.length = 1;
		ctrl.blocks = 1;
		GetBuffer()[0] = (BYTE)GetStatus();

		return;
	}

	Send();
}

void ScsiController::MsgIn()
{
	if (!IsMsgIn()) {
		LOGTRACE("%s Message In phase", __PRETTY_FUNCTION__)

		SetPhase(BUS::phase_t::msgin);

		bus.SetMSG(true);
		bus.SetCD(true);
		bus.SetIO(true);

		// length, blocks are already set
		assert(HasValidLength());
		assert(ctrl.blocks > 0);
		ResetOffset();
		return;
	}

	LOGTRACE("%s Transitioning to Send()", __PRETTY_FUNCTION__)
	Send();
}

void ScsiController::MsgOut()
{
	LOGTRACE("%s ID %d",__PRETTY_FUNCTION__, GetTargetId())

	if (!IsMsgOut()) {
		LOGTRACE("Message Out Phase")

	    // process the IDENTIFY message
		if (IsSelection()) {
			scsi.atnmsg = true;
			scsi.msc = 0;
			scsi.msb = {};
		}

		SetPhase(BUS::phase_t::msgout);

		bus.SetMSG(true);
		bus.SetCD(true);
		bus.SetIO(false);

		// Data transfer is 1 byte x 1 block
		ResetOffset();
		ctrl.length = 1;
		ctrl.blocks = 1;

		return;
	}

	Receive();
}

void ScsiController::DataIn()
{
	if (!IsDataIn()) {
		// Minimum execution time
		if (execstart > 0) {
			Sleep();
		}

		// If the length is 0, go to the status phase
		if (!HasValidLength()) {
			Status();
			return;
		}

		LOGTRACE("%s Going into Data-in Phase", __PRETTY_FUNCTION__)

		SetPhase(BUS::phase_t::datain);

		bus.SetMSG(false);
		bus.SetCD(false);
		bus.SetIO(true);

		// length, blocks are already set
		assert(ctrl.blocks > 0);
		ResetOffset();

		return;
	}

	Send();
}

void ScsiController::DataOut()
{
	if (!IsDataOut()) {
		// Minimum execution time
		if (execstart > 0) {
			Sleep();
		}

		// If the length is 0, go to the status phase
		if (!HasValidLength()) {
			Status();
			return;
		}

		LOGTRACE("%s Data out phase", __PRETTY_FUNCTION__)

		SetPhase(BUS::phase_t::dataout);

		// Signal line operated by the target
		bus.SetMSG(false);
		bus.SetCD(false);
		bus.SetIO(false);

		ResetOffset();
		return;
	}

	Receive();
}

void ScsiController::Error(sense_key sense_key, asc asc, status status)
{
	// Get bus information
	bus.Acquire();

	// Reset check
	if (bus.GetRST()) {
		Reset();
		bus.Reset();

		return;
	}

	// Bus free for status phase and message in phase
	if (IsStatus() || IsMsgIn()) {
		BusFree();
		return;
	}

	int lun = GetEffectiveLun();
	if (!HasDeviceForLun(lun) || asc == asc::INVALID_LUN) {
		assert(HasDeviceForLun(0));

		lun = 0;
	}

	if (sense_key != sense_key::NO_SENSE || asc != asc::NO_ADDITIONAL_SENSE_INFORMATION) {
		LOGDEBUG("Error status: Sense Key $%02X, ASC $%02X", (int)sense_key, (int)asc)

		// Set Sense Key and ASC for a subsequent REQUEST SENSE
		GetDeviceForLun(lun)->SetStatusCode(((int)sense_key << 16) | ((int)asc << 8));
	}

	SetStatus(status);
	ctrl.message = 0x00;

	LOGTRACE("%s Error (to status phase)", __PRETTY_FUNCTION__)

	Status();
}

void ScsiController::Send()
{
	assert(!bus.GetREQ());
	assert(bus.GetIO());

	if (HasValidLength()) {
		LOGTRACE("%s%s", __PRETTY_FUNCTION__, (" Sending handhake with offset " + to_string(GetOffset()) + ", length "
				+ to_string(ctrl.length)).c_str())

		// TODO The delay has to be taken from ctrl.unit[lun], but as there are currently no Daynaport drivers for
		// LUNs other than 0 this work-around works.
		if (const int len = bus.SendHandShake(GetBuffer().data() + ctrl.offset, ctrl.length,
				HasDeviceForLun(0) ? GetDeviceForLun(0)->GetSendDelay() : 0);
			len != (int)ctrl.length) {
			// If you cannot send all, move to status phase
			Error(sense_key::ABORTED_COMMAND);
			return;
		}

		UpdateOffsetAndLength();

		return;
	}

	// Block subtraction, result initialization
	ctrl.blocks--;
	bool result = true;

	// Processing after data collection (read/data-in only)
	if (IsDataIn() && ctrl.blocks != 0) {
		// set next buffer (set offset, length)
		result = XferIn(GetBuffer());
		LOGTRACE("%s%s", __PRETTY_FUNCTION__, (" Processing after data collection. Blocks: " + to_string(ctrl.blocks)).c_str())
	}

	// If result FALSE, move to status phase
	if (!result) {
		Error(sense_key::ABORTED_COMMAND);
		return;
	}

	// Continue sending if block !=0
	if (ctrl.blocks != 0){
		LOGTRACE("%s%s", __PRETTY_FUNCTION__, (" Continuing to send. Blocks: " + to_string(ctrl.blocks)).c_str())
		assert(HasValidLength());
		assert(ctrl.offset == 0);
		return;
	}

	// Move to next phase
	LOGTRACE("%s Move to next phase: %s", __PRETTY_FUNCTION__, BUS::GetPhaseStrRaw(GetPhase()))
	switch (GetPhase()) {
		// Message in phase
		case BUS::phase_t::msgin:
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
		case BUS::phase_t::datain:
			// status phase
			Status();
			break;

		// status phase
		case BUS::phase_t::status:
			// Message in phase
			ctrl.length = 1;
			ctrl.blocks = 1;
			GetBuffer()[0] = (BYTE)ctrl.message;
			MsgIn();
			break;

		default:
			assert(false);
			break;
	}
}

void ScsiController::Receive()
{
	if (is_byte_transfer) {
		ReceiveBytes();
		return;
	}

	LOGTRACE("%s",__PRETTY_FUNCTION__)

	// REQ is low
	assert(!bus.GetREQ());
	assert(!bus.GetIO());

	// Length != 0 if received
	if (HasValidLength()) {
		LOGTRACE("%s Length is %d bytes", __PRETTY_FUNCTION__, ctrl.length)

		// If not able to receive all, move to status phase
		if (int len = bus.ReceiveHandShake(GetBuffer().data() + GetOffset(), ctrl.length);
				len != (int)ctrl.length) {
			LOGERROR("%s Not able to receive %d bytes of data, only received %d",__PRETTY_FUNCTION__, ctrl.length, len)
			Error(sense_key::ABORTED_COMMAND);
			return;
		}

		UpdateOffsetAndLength();

		return;
	}

	// Block subtraction, result initialization
	ctrl.blocks--;
	bool result = true;

	// Processing after receiving data (by phase)
	LOGTRACE("%s Phase: %s",__PRETTY_FUNCTION__, BUS::GetPhaseStrRaw(GetPhase()))
	switch (GetPhase()) {
		case BUS::phase_t::dataout:
			if (ctrl.blocks == 0) {
				// End with this buffer
				result = XferOut(false);
			} else {
				// Continue to next buffer (set offset, length)
				result = XferOut(true);
			}
			break;

		case BUS::phase_t::msgout:
			ctrl.message = GetBuffer()[0];
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
	// TODO Check whether we can raise scsi_exception here in order to simplify the error handling
	if (!result) {
		Error(sense_key::ABORTED_COMMAND);
		return;
	}

	// Continue to receive if block !=0
	if (ctrl.blocks != 0) {
		assert(HasValidLength());
		assert(ctrl.offset == 0);
		return;
	}

	// Move to next phase
	switch (GetPhase()) {
		case BUS::phase_t::command:
			ProcessCommand();
			break;

		case BUS::phase_t::msgout:
			ProcessMessage();
			break;

		case BUS::phase_t::dataout:
			// Block-oriented data have been handled above
			DataOutNonBlockOriented();

			Status();
			break;

		default:
			assert(false);
			break;
	}
}

bool ScsiController::XferMsg(int msg)
{
	assert(IsMsgOut());

	// Save message out data
	if (scsi.atnmsg) {
		scsi.msb[scsi.msc] = (BYTE)msg;
		scsi.msc++;
		scsi.msc %= 256;
	}

	return true;
}

void ScsiController::ReceiveBytes()
{
	assert(!bus.GetREQ());
	assert(!bus.GetIO());

	if (HasValidLength()) {
		LOGTRACE("%s Length is %d bytes", __PRETTY_FUNCTION__, ctrl.length)

		// If not able to receive all, move to status phase
		if (uint32_t len = bus.ReceiveHandShake(GetBuffer().data() + GetOffset(), ctrl.length);
				len != ctrl.length) {
			LOGERROR("%s Not able to receive %d bytes of data, only received %d",
					__PRETTY_FUNCTION__, ctrl.length, len)
			Error(sense_key::ABORTED_COMMAND);
			return;
		}

		bytes_to_transfer = ctrl.length;

		UpdateOffsetAndLength();

		return;
	}

	// Result initialization
	bool result = true;

	// Processing after receiving data (by phase)
	LOGTRACE("%s Phase: %s",__PRETTY_FUNCTION__, BUS::GetPhaseStrRaw(GetPhase()))
	switch (GetPhase()) {
		case BUS::phase_t::dataout:
			result = XferOut(false);
			break;

		case BUS::phase_t::msgout:
			ctrl.message = GetBuffer()[0];
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
		Error(sense_key::ABORTED_COMMAND);
		return;
	}

	// Move to next phase
	switch (GetPhase()) {
		case BUS::phase_t::command:
			ProcessCommand();
			break;

		case BUS::phase_t::msgout:
			ProcessMessage();
			break;

		case BUS::phase_t::dataout:
			Status();
			break;

		default:
			assert(false);
			break;
	}
}

bool ScsiController::XferOut(bool cont)
{
	assert(IsDataOut());

	if (!is_byte_transfer) {
		return XferOutBlockOriented(cont);
	}

	is_byte_transfer = false;

	if (auto device = GetDeviceForLun(GetEffectiveLun());
		device != nullptr && GetOpcode() == scsi_command::eCmdWrite6) {
		return device->WriteByteSequence(GetBuffer(), bytes_to_transfer);
	}

	LOGWARN("%s Received unexpected command $%02X", __PRETTY_FUNCTION__, (int)GetOpcode())

	return false;
}

void ScsiController::DataOutNonBlockOriented()
{
	assert(IsDataOut());

	switch (GetOpcode()) {
		// TODO Check why these cases are needed
		case scsi_command::eCmdWrite6:
		case scsi_command::eCmdWrite10:
		case scsi_command::eCmdWrite16:
		case scsi_command::eCmdWriteLong10:
		case scsi_command::eCmdWriteLong16:
		case scsi_command::eCmdVerify10:
		case scsi_command::eCmdVerify16:
			break;

		case scsi_command::eCmdModeSelect6:
		case scsi_command::eCmdModeSelect10: {
				// TODO Try to get rid of this cast
				if (auto device = dynamic_pointer_cast<ModePageDevice>(GetDeviceForLun(GetEffectiveLun()));
					device != nullptr) {
					device->ModeSelect(ctrl.cmd, GetBuffer(), GetOffset());
				}
				else {
					throw scsi_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_COMMAND_OPERATION_CODE);
				}
			}
			break;

		case scsi_command::eCmdSetMcastAddr:
			// TODO: Eventually, we should store off the multicast address configuration data here...
			break;

		default:
			LOGWARN("Unexpected Data Out phase for command $%02X", (int)GetOpcode())
			break;
	}
}

//---------------------------------------------------------------------------
//
//	Data transfer IN
//	*Reset offset and length
//
//---------------------------------------------------------------------------
bool ScsiController::XferIn(vector<BYTE>& buf)
{
	assert(IsDataIn());

	LOGTRACE("%s command=%02X", __PRETTY_FUNCTION__, (int)GetOpcode())

	int lun = GetEffectiveLun();
	if (!HasDeviceForLun(lun)) {
		return false;
	}

	// Limited to read commands
	switch (GetOpcode()) {
		case scsi_command::eCmdRead6:
		case scsi_command::eCmdRead10:
		case scsi_command::eCmdRead16:
			// Read from disk
			try {
				ctrl.length = (dynamic_pointer_cast<Disk>(GetDeviceForLun(lun)))->Read(ctrl.cmd, buf, ctrl.next);
			}
			catch(const scsi_exception&) {
				// If there is an error, go to the status phase
				return false;
			}

			ctrl.next++;

			// If things are normal, work setting
			ResetOffset();
			break;

		// Other (impossible)
		default:
			assert(false);
			return false;
	}

	return true;
}

//---------------------------------------------------------------------------
//
//	Data transfer OUT
//	*If cont=true, reset the offset and length
//
//---------------------------------------------------------------------------
bool ScsiController::XferOutBlockOriented(bool cont)
{
	auto disk = dynamic_pointer_cast<Disk>(GetDeviceForLun(GetEffectiveLun()));
	if (disk == nullptr) {
		return false;
	}

	// Limited to write commands
	switch (GetOpcode()) {
		case scsi_command::eCmdModeSelect6:
		case scsi_command::eCmdModeSelect10:
			try {
				disk->ModeSelect(ctrl.cmd, GetBuffer(), GetOffset());
			}
			catch(const scsi_exception& e) {
				Error(e.get_sense_key(), e.get_asc(), e.get_status());
				return false;
			}
			break;

		case scsi_command::eCmdWrite6:
		case scsi_command::eCmdWrite10:
		case scsi_command::eCmdWrite16:
		// TODO Verify has to verify, not to write
		case scsi_command::eCmdVerify10:
		case scsi_command::eCmdVerify16:
		{
			// Special case Write function for brige
			// TODO This class must not know about SCSIBR
			if (auto bridge = dynamic_pointer_cast<SCSIBR>(disk); bridge) {
				if (!bridge->WriteBytes(ctrl.cmd, GetBuffer(), ctrl.length)) {
					// Write failed
					return false;
				}

				ResetOffset();
				break;
			}

			// Special case Write function for DaynaPort
			// TODO This class must not know about DaynaPort
			if (auto daynaport = dynamic_pointer_cast<SCSIDaynaPort>(disk); daynaport) {
				daynaport->WriteBytes(ctrl.cmd, GetBuffer(), 0);

				ResetOffset();
				ctrl.blocks = 0;
				break;
			}

			try {
				disk->Write(ctrl.cmd, GetBuffer(), ctrl.next - 1);
			}
			catch(const scsi_exception& e) {
				Error(e.get_sense_key(), e.get_asc(), e.get_status());

				// Write failed
				return false;
			}

			// If you do not need the next block, end here
			ctrl.next++;
			if (!cont) {
				break;
			}

			// Check the next block
			try {
				ctrl.length = disk->WriteCheck(ctrl.next - 1);
			}
			catch(const scsi_exception&) {
				// Cannot write
				return false;
			}

			ResetOffset();
			break;
		}

		case scsi_command::eCmdSetMcastAddr:
			LOGTRACE("%s Done with DaynaPort Set Multicast Address", __PRETTY_FUNCTION__)
			break;

		default:
			LOGWARN("Received an unexpected command ($%02X) in %s", (int)GetOpcode(), __PRETTY_FUNCTION__)
			break;
	}

	return true;
}

void ScsiController::ProcessCommand()
{
	uint32_t len = GPIOBUS::GetCommandByteCount(GetBuffer()[0]);

	stringstream s;
	s << setfill('0') << setw(2) << hex;
	for (uint32_t i = 0; i < len; i++) {
		ctrl.cmd[i] = GetBuffer()[i];
		s << ctrl.cmd[i];
	}
	LOGTRACE("%s CDB=$%s",__PRETTY_FUNCTION__, s.str().c_str())

	Execute();
}

void ScsiController::ParseMessage()
{
	int i = 0;
	while (i < scsi.msc) {
		const BYTE message_type = scsi.msb[i];

		if (message_type == 0x06) {
			LOGTRACE("Received ABORT message")
			BusFree();
			return;
		}

		if (message_type == 0x0C) {
			LOGTRACE("Received BUS DEVICE RESET message")
			scsi.syncoffset = 0;
			BusFree();
			return;
		}

		if (message_type >= 0x80) {
			identified_lun = (int)message_type & 0x1F;
			LOGTRACE("Received IDENTIFY message for LUN %d", identified_lun)
		}

		if (message_type == 0x01) {
			LOGTRACE("Received EXTENDED MESSAGE")

			// Check only when synchronous transfer is possible
			if (!scsi.syncenable || scsi.msb[i + 2] != 0x01) {
				ctrl.length = 1;
				ctrl.blocks = 1;
				GetBuffer()[0] = 0x07;
				MsgIn();
				return;
			}

			scsi.syncperiod = scsi.msb[i + 3];
			if (scsi.syncperiod > MAX_SYNC_PERIOD) {
				scsi.syncperiod = MAX_SYNC_PERIOD;
			}

			scsi.syncoffset = scsi.msb[i + 4];
			if (scsi.syncoffset > MAX_SYNC_OFFSET) {
				scsi.syncoffset = MAX_SYNC_OFFSET;
			}

			// STDR response message generation
			ctrl.length = 5;
			ctrl.blocks = 1;
			GetBuffer()[0] = 0x01;
			GetBuffer()[1] = 0x03;
			GetBuffer()[2] = 0x01;
			GetBuffer()[3] = scsi.syncperiod;
			GetBuffer()[4] = scsi.syncoffset;
			MsgIn();
			return;
		}

		// Next message
		i++;
	}
}

void ScsiController::ProcessMessage()
{
	// Continue message out phase as long as ATN keeps asserting
	if (bus.GetATN()) {
		// Data transfer is 1 byte x 1 block
		ResetOffset();
		ctrl.length = 1;
		ctrl.blocks = 1;
		return;
	}

	if (scsi.atnmsg) {
		ParseMessage();
	}

	// Initialize ATN message reception status
	scsi.atnmsg = false;

	Command();
}

int ScsiController::GetEffectiveLun() const
{
	// Return LUN from IDENTIFY message, or return the LUN from the CDB as fallback
	return identified_lun != -1 ? identified_lun : GetLun();
}

void ScsiController::Sleep()
{
	if (const uint32_t time = SysTimer::GetTimerLow() - execstart; time < MIN_EXEC_TIME) {
		SysTimer::SleepUsec(MIN_EXEC_TIME - time);
	}
	execstart = 0;
}

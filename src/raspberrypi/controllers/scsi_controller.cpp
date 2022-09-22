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
//  	[ SCSI device controller ]
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
#ifdef __linux
#include <linux/if_tun.h>
#endif

using namespace scsi_defs;

ScsiController::ScsiController(shared_ptr<BUS> bus, int target_id) : AbstractController(bus, target_id)
{
	// The initial buffer size will default to either the default buffer size OR
	// the size of an Ethernet message, whichever is larger.
	ctrl.bufsize = std::max(DEFAULT_BUFFER_SIZE, ETH_FRAME_LEN + 16 + ETH_FCS_LEN);
	ctrl.buffer = new BYTE[ctrl.bufsize];
}

ScsiController::~ScsiController()
{
	delete[] ctrl.buffer;
}

void ScsiController::Reset()
{
	SetPhase(BUS::phase_t::busfree);
	ctrl.status = 0x00;
	ctrl.message = 0x00;
	execstart = 0;
	ctrl.blocks = 0;
	ctrl.next = 0;
	ctrl.offset = 0;
	ctrl.length = 0;
	identified_lun = -1;

	scsi.atnmsg = false;
	scsi.msc = 0;
	memset(scsi.msb.data(), 0x00, scsi.msb.size());

	is_byte_transfer = false;
	bytes_to_transfer = 0;

	// Reset all LUNs
	for (const auto& [lun, device] : ctrl.luns) {
		device->Reset();
	}
}

BUS::phase_t ScsiController::Process(int id)
{
	// Get bus information
	bus->Acquire();

	// Check to see if the reset signal was asserted
	if (bus->GetRST()) {
		LOGWARN("RESET signal received!")

		// Reset the controller
		Reset();

		// Reset the bus
		bus->Reset();

		return ctrl.phase;
	}

	if (id != UNKNOWN_INITIATOR_ID) {
		LOGTRACE("%s Initiator ID is %d", __PRETTY_FUNCTION__, id)
	}
	else {
		LOGTRACE("%s Initiator ID is unknown", __PRETTY_FUNCTION__)
	}

	initiator_id = id;

	try {
		// Phase processing
		switch (ctrl.phase) {
			case BUS::phase_t::busfree:
				BusFree();
				break;

			case BUS::phase_t::selection:
				Selection();
				break;

			case BUS::phase_t::dataout:
				DataOut();
				break;

			case BUS::phase_t::datain:
				DataIn();
				break;

			case BUS::phase_t::command:
				Command();
				break;

			case BUS::phase_t::status:
				Status();
				break;

			case BUS::phase_t::msgout:
				MsgOut();
				break;

			case BUS::phase_t::msgin:
				MsgIn();
				break;

			default:
				assert(false);
				break;
		}
	}
	catch(const scsi_error_exception&) {
		// Any exception should have been handled during the phase processing
		assert(false);

		LOGERROR("%s Unhandled SCSI error, resetting controller and bus and entering bus free phase", __PRETTY_FUNCTION__)

		Reset();
		bus->Reset();

		BusFree();
	}

	return ctrl.phase;
}

void ScsiController::BusFree()
{
	if (ctrl.phase != BUS::phase_t::busfree) {
		LOGTRACE("%s Bus free phase", __PRETTY_FUNCTION__)

		SetPhase(BUS::phase_t::busfree);

		bus->SetREQ(false);
		bus->SetMSG(false);
		bus->SetCD(false);
		bus->SetIO(false);
		bus->SetBSY(false);

		// Initialize status and message
		ctrl.status = 0x00;
		ctrl.message = 0x00;

		// Initialize ATN message reception status
		scsi.atnmsg = false;

		identified_lun = -1;

		is_byte_transfer = false;
		bytes_to_transfer = 0;

		// When the bus is free RaSCSI or the Pi may be shut down.
		// TODO Try to find a better place for this code without breaking encapsulation
		switch(shutdown_mode) {
		case AbstractController::rascsi_shutdown_mode::STOP_RASCSI:
			LOGINFO("RaSCSI shutdown requested")
			exit(0);
			break;

		case AbstractController::rascsi_shutdown_mode::STOP_PI:
			LOGINFO("Raspberry Pi shutdown requested")
			if (system("init 0") == -1) {
				LOGERROR("Raspberry Pi shutdown failed: %s", strerror(errno))
			}
			break;

		case AbstractController::rascsi_shutdown_mode::RESTART_PI:
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
	if (bus->GetSEL() && !bus->GetBSY()) {
		Selection();
	}
}

void ScsiController::Selection()
{
	if (ctrl.phase != BUS::phase_t::selection) {
		// A different device controller was selected
		if (int id = 1 << GetTargetId(); ((int)bus->GetDAT() & id) == 0) {
			return;
		}

		// Abort if there is no LUN for this controller
		if (ctrl.luns.empty()) {
			return;
		}

		LOGTRACE("%s Selection Phase Target ID=%d", __PRETTY_FUNCTION__, GetTargetId())

		SetPhase(BUS::phase_t::selection);

		// Raise BSY and respond
		bus->SetBSY(true);
		return;
	}

	// Selection completed
	if (!bus->GetSEL() && bus->GetBSY()) {
		// Message out phase if ATN=1, otherwise command phase
		if (bus->GetATN()) {
			MsgOut();
		} else {
			Command();
		}
	}
}

void ScsiController::Command()
{
	if (ctrl.phase != BUS::phase_t::command) {
		LOGTRACE("%s Command Phase", __PRETTY_FUNCTION__)

		SetPhase(BUS::phase_t::command);

		bus->SetMSG(false);
		bus->SetCD(true);
		bus->SetIO(false);

		int actual_count = bus->CommandHandShake(ctrl.buffer);
		int command_byte_count = GPIOBUS::GetCommandByteCount(ctrl.buffer[0]);

		// If not able to receive all, move to the status phase
		if (actual_count != command_byte_count) {
			LOGERROR("%s Command byte count mismatch: expected %d bytes, received %d byte(s)", __PRETTY_FUNCTION__,
					command_byte_count, actual_count)
			Error(sense_key::ABORTED_COMMAND);
			return;
		}

		ctrl.cmd.resize(command_byte_count);

		// Command data transfer
		stringstream s;
		for (int i = 0; i < command_byte_count; i++) {
			ctrl.cmd[i] = ctrl.buffer[i];
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

	SetPhase(BUS::phase_t::execute);

	// Initialization for data transfer
	ctrl.offset = 0;
	ctrl.blocks = 1;
	execstart = SysTimer::GetTimerLow();

	// Discard pending sense data from the previous command if the current command is not REQUEST SENSE
	if (GetOpcode() != scsi_command::eCmdRequestSense) {
		ctrl.status = 0;
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

	PrimaryDevice *device = GetDeviceForLun(lun);

	// Discard pending sense data from the previous command if the current command is not REQUEST SENSE
	if (GetOpcode() != scsi_command::eCmdRequestSense) {
		device->SetStatusCode(0);
	}
	
	try {
		if (!device->Dispatch(GetOpcode())) {
			LOGTRACE("ID %d LUN %d received unsupported command: $%02X", GetTargetId(), lun, (int)GetOpcode())

			throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_COMMAND_OPERATION_CODE);
		}
	}
	catch(const scsi_error_exception& e) { //NOSONAR This exception is handled properly
		Error(e.get_sense_key(), e.get_asc(), e.get_status());

		// Fall through
	}

	// SCSI-2 p.104 4.4.3 Incorrect logical unit handling
	if (GetOpcode() == scsi_command::eCmdInquiry && !HasDeviceForLun(lun)) {
		lun = GetEffectiveLun();

		LOGTRACE("Reporting LUN %d for device ID %d as not supported", lun, device->GetId())

		ctrl.buffer[0] = 0x7f;
	}
}

void ScsiController::Status()
{
	if (ctrl.phase != BUS::phase_t::status) {
		// Minimum execution time
		if (execstart > 0) {
			Sleep();
		} else {
			SysTimer::SleepUsec(5);
		}

		LOGTRACE("%s Status Phase $%02X",__PRETTY_FUNCTION__, ctrl.status)

		SetPhase(BUS::phase_t::status);

		// Signal line operated by the target
		bus->SetMSG(false);
		bus->SetCD(true);
		bus->SetIO(true);

		// Data transfer is 1 byte x 1 block
		ctrl.offset = 0;
		ctrl.length = 1;
		ctrl.blocks = 1;
		ctrl.buffer[0] = (BYTE)ctrl.status;

		return;
	}

	Send();
}

void ScsiController::MsgIn()
{
	if (ctrl.phase != BUS::phase_t::msgin) {
		LOGTRACE("%s Message In phase", __PRETTY_FUNCTION__)

		SetPhase(BUS::phase_t::msgin);

		bus->SetMSG(true);
		bus->SetCD(true);
		bus->SetIO(true);

		// length, blocks are already set
		assert(ctrl.length > 0);
		assert(ctrl.blocks > 0);
		ctrl.offset = 0;
		return;
	}

	LOGTRACE("%s Transitioning to Send()", __PRETTY_FUNCTION__)
	Send();
}

void ScsiController::MsgOut()
{
	LOGTRACE("%s ID %d",__PRETTY_FUNCTION__, GetTargetId())

	if (ctrl.phase != BUS::phase_t::msgout) {
		LOGTRACE("Message Out Phase")

	    // process the IDENTIFY message
		if (ctrl.phase == BUS::phase_t::selection) {
			scsi.atnmsg = true;
			scsi.msc = 0;
			memset(scsi.msb.data(), 0x00, scsi.msb.size());
		}

		SetPhase(BUS::phase_t::msgout);

		bus->SetMSG(true);
		bus->SetCD(true);
		bus->SetIO(false);

		// Data transfer is 1 byte x 1 block
		ctrl.offset = 0;
		ctrl.length = 1;
		ctrl.blocks = 1;

		return;
	}

	Receive();
}

void ScsiController::DataIn()
{
	if (ctrl.phase != BUS::phase_t::datain) {
		// Minimum execution time
		if (execstart > 0) {
			Sleep();
		}

		// If the length is 0, go to the status phase
		if (ctrl.length == 0) {
			Status();
			return;
		}

		LOGTRACE("%s Going into Data-in Phase", __PRETTY_FUNCTION__)

		SetPhase(BUS::phase_t::datain);

		bus->SetMSG(false);
		bus->SetCD(false);
		bus->SetIO(true);

		// length, blocks are already set
		assert(ctrl.blocks > 0);
		ctrl.offset = 0;

		return;
	}

	Send();
}

void ScsiController::DataOut()
{
	if (ctrl.phase != BUS::phase_t::dataout) {
		// Minimum execution time
		if (execstart > 0) {
			Sleep();
		}

		// If the length is 0, go to the status phase
		if (ctrl.length == 0) {
			Status();
			return;
		}

		LOGTRACE("%s Data out phase", __PRETTY_FUNCTION__)

		SetPhase(BUS::phase_t::dataout);

		// Signal line operated by the target
		bus->SetMSG(false);
		bus->SetCD(false);
		bus->SetIO(false);

		ctrl.offset = 0;
		return;
	}

	Receive();
}

void ScsiController::Error(sense_key sense_key, asc asc, status status)
{
	// Get bus information
	bus->Acquire();

	// Reset check
	if (bus->GetRST()) {
		Reset();
		bus->Reset();

		return;
	}

	// Bus free for status phase and message in phase
	if (ctrl.phase == BUS::phase_t::status || ctrl.phase == BUS::phase_t::msgin) {
		BusFree();
		return;
	}

	int lun = GetEffectiveLun();
	if (!HasDeviceForLun(lun) || asc == asc::INVALID_LUN) {
		assert(HasDeviceForLun(0));

		lun = 0;
	}

	if (sense_key != sense_key::NO_SENSE || asc != asc::NO_ADDITIONAL_SENSE_INFORMATION) {
		// Set Sense Key and ASC for a subsequent REQUEST SENSE
		GetDeviceForLun(lun)->SetStatusCode(((int)sense_key << 16) | ((int)asc << 8));
	}

	ctrl.status = (uint32_t)status;
	ctrl.message = 0x00;

	LOGTRACE("%s Error (to status phase)", __PRETTY_FUNCTION__)

	Status();
}

void ScsiController::Send()
{
	assert(!bus->GetREQ());
	assert(bus->GetIO());

	if (ctrl.length != 0) {
		LOGTRACE("%s%s", __PRETTY_FUNCTION__, (" Sending handhake with offset " + to_string(ctrl.offset) + ", length "
				+ to_string(ctrl.length)).c_str())

		// TODO The delay has to be taken from ctrl.unit[lun], but as there are currently no Daynaport drivers for
		// LUNs other than 0 this work-around works.
		if (int len = bus->SendHandShake(&ctrl.buffer[ctrl.offset], ctrl.length,
				HasDeviceForLun(0) ? GetDeviceForLun(0)->GetSendDelay() : 0);
			len != (int)ctrl.length) {
			// If you cannot send all, move to status phase
			Error(sense_key::ABORTED_COMMAND);
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
	if (ctrl.phase == BUS::phase_t::datain && ctrl.blocks != 0) {
		// set next buffer (set offset, length)
		result = XferIn(ctrl.buffer);
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
		assert(ctrl.length > 0);
		assert(ctrl.offset == 0);
		return;
	}

	// Move to next phase
	LOGTRACE("%s Move to next phase %s (%d)", __PRETTY_FUNCTION__, BUS::GetPhaseStrRaw(ctrl.phase), (int)ctrl.phase)
	switch (ctrl.phase) {
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
			ctrl.buffer[0] = (BYTE)ctrl.message;
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
	assert(!bus->GetREQ());
	assert(!bus->GetIO());

	// Length != 0 if received
	if (ctrl.length != 0) {
		LOGTRACE("%s Length is %d bytes", __PRETTY_FUNCTION__, ctrl.length)
		// Receive
		int len = bus->ReceiveHandShake(&ctrl.buffer[ctrl.offset], ctrl.length);

		// If not able to receive all, move to status phase
		if (len != (int)ctrl.length) {
			LOGERROR("%s Not able to receive %d bytes of data, only received %d",__PRETTY_FUNCTION__, ctrl.length, len)
			Error(sense_key::ABORTED_COMMAND);
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
	LOGTRACE("%s ctrl.phase: %d (%s)",__PRETTY_FUNCTION__, (int)ctrl.phase, BUS::GetPhaseStrRaw(ctrl.phase))
	switch (ctrl.phase) {
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
		Error(sense_key::ABORTED_COMMAND);
		return;
	}

	// Continue to receive if block !=0
	if (ctrl.blocks != 0) {
		assert(ctrl.length > 0);
		assert(ctrl.offset == 0);
		return;
	}

	// Move to next phase
	switch (ctrl.phase) {
		case BUS::phase_t::command: {
			int len = GPIOBUS::GetCommandByteCount(ctrl.buffer[0]);

			for (int i = 0; i < len; i++) {
				ctrl.cmd[i] = ctrl.buffer[i];
				LOGTRACE("%s Command Byte %d: $%02X",__PRETTY_FUNCTION__, i, ctrl.cmd[i])
			}

			Execute();
			break;
		}

		case BUS::phase_t::msgout:
			// Continue message out phase as long as ATN keeps asserting
			if (bus->GetATN()) {
				// Data transfer is 1 byte x 1 block
				ctrl.offset = 0;
				ctrl.length = 1;
				ctrl.blocks = 1;
				return;
			}

			if (ProcessMessage()) {
				return;
			}

			break;

		case BUS::phase_t::dataout:
			FlushUnit();

			Status();
			break;

		default:
			assert(false);
			break;
	}
}

bool ScsiController::XferMsg(int msg)
{
	assert(ctrl.phase == BUS::phase_t::msgout);

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
	LOGTRACE("%s",__PRETTY_FUNCTION__)

	// REQ is low
	assert(!bus->GetREQ());
	assert(!bus->GetIO());

	if (ctrl.length) {
		LOGTRACE("%s Length is %d bytes", __PRETTY_FUNCTION__, ctrl.length)

		uint32_t len = bus->ReceiveHandShake(&ctrl.buffer[ctrl.offset], ctrl.length);

		// If not able to receive all, move to status phase
		if (len != ctrl.length) {
			LOGERROR("%s Not able to receive %d bytes of data, only received %d",
					__PRETTY_FUNCTION__, ctrl.length, len)
			Error(sense_key::ABORTED_COMMAND);
			return;
		}

		bytes_to_transfer = ctrl.length;

		ctrl.offset += ctrl.length;
		ctrl.length = 0;

		return;
	}

	// Result initialization
	bool result = true;

	// Processing after receiving data (by phase)
	LOGTRACE("%s ctrl.phase: %d (%s)",__PRETTY_FUNCTION__, (int)ctrl.phase, BUS::GetPhaseStrRaw(ctrl.phase))
	switch (ctrl.phase) {

		case BUS::phase_t::dataout:
			result = XferOut(false);
			break;

		case BUS::phase_t::msgout:
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
		Error(sense_key::ABORTED_COMMAND);
		return;
	}

	// Move to next phase
	switch (ctrl.phase) {
		case BUS::phase_t::command: {
			uint32_t len = GPIOBUS::GetCommandByteCount(ctrl.buffer[0]);

			for (uint32_t i = 0; i < len; i++) {
				ctrl.cmd[i] = ctrl.buffer[i];
				LOGTRACE("%s Command Byte %d: $%02X",__PRETTY_FUNCTION__, i, ctrl.cmd[i])
			}

			Execute();
			break;
		}

		case BUS::phase_t::msgout:
			// Continue message out phase as long as ATN keeps asserting
			if (bus->GetATN()) {
				// Data transfer is 1 byte x 1 block
				ctrl.offset = 0;
				ctrl.length = 1;
				ctrl.blocks = 1;
				return;
			}

			if (ProcessMessage()) {
				return;
			}

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
	assert(ctrl.phase == BUS::phase_t::dataout);

	if (!is_byte_transfer) {
		return XferOutBlockOriented(cont);
	}

	is_byte_transfer = false;

	if (auto device = GetDeviceForLun(GetEffectiveLun());
		device != nullptr && GetOpcode() == scsi_command::eCmdWrite6) {
		return device->WriteByteSequence(ctrl.buffer, bytes_to_transfer);
	}

	LOGWARN("Received an unexpected command ($%02X) in %s", (int)GetOpcode(), __PRETTY_FUNCTION__)

	return false;
}

void ScsiController::FlushUnit()
{
	assert(ctrl.phase == BUS::phase_t::dataout);

	auto disk = dynamic_cast<Disk *>(GetDeviceForLun(GetEffectiveLun()));
	if (disk == nullptr) {
		return;
	}

	// WRITE system only
	switch (GetOpcode()) {
		case scsi_command::eCmdWrite6:
		case scsi_command::eCmdWrite10:
		case scsi_command::eCmdWrite16:
		case scsi_command::eCmdWriteLong10:
		case scsi_command::eCmdWriteLong16:
		case scsi_command::eCmdVerify10:
		case scsi_command::eCmdVerify16:
			break;

		case scsi_command::eCmdModeSelect6:
		case scsi_command::eCmdModeSelect10:
            // TODO What is this special handling of ModeSelect good for?
            // Without it we would not need this method at all.
            // ModeSelect is already handled in XferOutBlockOriented(). Why would it have to be handled once more?
			try {
				disk->ModeSelect(ctrl.cmd, ctrl.buffer, ctrl.offset);
			}
			catch(const scsi_error_exception& e) {
				LOGWARN("Error occured while processing Mode Select command %02X\n", (int)GetOpcode())
				Error(e.get_sense_key(), e.get_asc(), e.get_status());
				return;
			}
            break;

		case scsi_command::eCmdSetMcastAddr:
			// TODO: Eventually, we should store off the multicast address configuration data here...
			break;

		default:
			LOGWARN("Received an unexpected flush command $%02X\n", (int)GetOpcode())
			break;
	}
}

//---------------------------------------------------------------------------
//
//	Data transfer IN
//	*Reset offset and length
//
//---------------------------------------------------------------------------
bool ScsiController::XferIn(BYTE *buf)
{
	assert(ctrl.phase == BUS::phase_t::datain);

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
				ctrl.length = (static_cast<Disk *>(GetDeviceForLun(lun)))->Read(ctrl.cmd, buf, ctrl.next);
			}
			catch(const scsi_error_exception&) {
				// If there is an error, go to the status phase
				return false;
			}

			ctrl.next++;

			// If things are normal, work setting
			ctrl.offset = 0;
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
	auto disk = dynamic_cast<Disk *>(GetDeviceForLun(GetEffectiveLun()));
	if (disk == nullptr) {
		return false;
	}

	// Limited to write commands
	switch (GetOpcode()) {
		case scsi_command::eCmdModeSelect6:
		case scsi_command::eCmdModeSelect10:
			try {
				disk->ModeSelect(ctrl.cmd, ctrl.buffer, ctrl.offset);
			}
			catch(const scsi_error_exception& e) {
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
			if (auto bridge = dynamic_cast<SCSIBR *>(disk); bridge) {
				if (!bridge->WriteBytes(ctrl.cmd, ctrl.buffer, ctrl.length)) {
					// Write failed
					return false;
				}

				ctrl.offset = 0;
				break;
			}

			// Special case Write function for DaynaPort
			// TODO This class must not know about DaynaPort
			if (auto daynaport = dynamic_cast<SCSIDaynaPort *>(disk); daynaport) {
				daynaport->WriteBytes(ctrl.cmd, ctrl.buffer, 0);

				ctrl.offset = 0;
				ctrl.blocks = 0;
				break;
			}

			try {
				disk->Write(ctrl.cmd, ctrl.buffer, ctrl.next - 1);
			}
			catch(const scsi_error_exception& e) {
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
			catch(const scsi_error_exception&) {
				// Cannot write
				return false;
			}

			ctrl.offset = 0;
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

bool ScsiController::ProcessMessage()
{
	// Parsing messages sent by ATN
	if (scsi.atnmsg) {
		int i = 0;
		while (i < scsi.msc) {
			// Message type
			BYTE data = scsi.msb[i];

			// ABORT
			if (data == 0x06) {
				LOGTRACE("Message code ABORT $%02X", data)
				BusFree();
				return true;
			}

			// BUS DEVICE RESET
			if (data == 0x0C) {
				LOGTRACE("Message code BUS DEVICE RESET $%02X", data)
				scsi.syncoffset = 0;
				BusFree();
				return true;
			}

			// IDENTIFY
			if (data >= 0x80) {
				identified_lun = data & 0x1F;
				LOGTRACE("Message code IDENTIFY $%02X, LUN %d selected", data, identified_lun)
			}

			// Extended Message
			if (data == 0x01) {
				LOGTRACE("Message code EXTENDED MESSAGE $%02X", data)

				// Check only when synchronous transfer is possible
				if (!scsi.syncenable || scsi.msb[i + 2] != 0x01) {
					ctrl.length = 1;
					ctrl.blocks = 1;
					ctrl.buffer[0] = 0x07;
					MsgIn();
					return true;
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
				ctrl.buffer[0] = 0x01;
				ctrl.buffer[1] = 0x03;
				ctrl.buffer[2] = 0x01;
				ctrl.buffer[3] = scsi.syncperiod;
				ctrl.buffer[4] = scsi.syncoffset;
				MsgIn();
				return true;
			}

			// next
			i++;
		}
	}

	// Initialize ATN message reception status
	scsi.atnmsg = false;

	Command();

	return false;
}

int ScsiController::GetEffectiveLun() const
{
	// Return LUN from IDENTIFY message, or return the LUN from the CDB as fallback
	return identified_lun != -1 ? identified_lun : GetLun();
}

void ScsiController::Sleep()
{
	if (uint32_t time = SysTimer::GetTimerLow() - execstart; time < MIN_EXEC_TIME) {
		SysTimer::SleepUsec(MIN_EXEC_TIME - time);
	}
	execstart = 0;
}

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

#include "shared/log.h"
#include "shared/rascsi_exceptions.h"
#include "hal/gpiobus.h"
#include "hal/systimer.h"
#include "devices/interfaces/byte_writer.h"
#include "devices/mode_page_device.h"
#include "devices/disk.h"
#include "scsi_controller.h"
#include <sstream>
#include <iomanip>
#ifdef __linux__
#include <linux/if_tun.h>
#endif

using namespace scsi_defs;

ScsiController::ScsiController(shared_ptr<ControllerManager> controller_manager, int target_id)
	: AbstractController(controller_manager, target_id, LUN_MAX)
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

	SetByteTransfer(false);
}

BUS::phase_t ScsiController::Process(int id)
{
	GetBus().Acquire();

	if (GetBus().GetRST()) {
		LOGWARN("RESET signal received!")

		Reset();

		GetBus().Reset();

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
		LOGERROR("%s Unhandled SCSI error, resetting controller and bus and entering bus free phase", __PRETTY_FUNCTION__)

		Reset();
		GetBus().Reset();

		BusFree();
	}

	return GetPhase();
}

void ScsiController::BusFree()
{
	if (!IsBusFree()) {
		LOGTRACE("%s Bus free phase", __PRETTY_FUNCTION__)

		SetPhase(BUS::phase_t::busfree);

		GetBus().SetREQ(false);
		GetBus().SetMSG(false);
		GetBus().SetCD(false);
		GetBus().SetIO(false);
		GetBus().SetBSY(false);

		// Initialize status and message
		SetStatus(status::GOOD);
		SetMessage(0x00);

		// Initialize ATN message reception status
		scsi.atnmsg = false;

		identified_lun = -1;

		SetByteTransfer(false);

		if (shutdown_mode != rascsi_shutdown_mode::NONE) {
			// Prepare the shutdown by flushing all caches
			for (const auto& device : GetControllerManager()->GetAllDevices()) {
				device->FlushCache();
			}
		}

		// When the bus is free RaSCSI or the Pi may be shut down.
		// This code has to be executed in the bus free phase and thus has to be located in the controller.
		switch(shutdown_mode) {
		case rascsi_shutdown_mode::STOP_RASCSI:
			LOGINFO("RaSCSI shutdown requested")
			exit(EXIT_SUCCESS);
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
	if (GetBus().GetSEL() && !GetBus().GetBSY()) {
		Selection();
	}
}

void ScsiController::Selection()
{
	if (!IsSelection()) {
		// A different device controller was selected
		if (int id = 1 << GetTargetId(); (static_cast<int>(GetBus().GetDAT()) & id) == 0) {
			return;
		}

		// Abort if there is no LUN for this controller
		if (!GetLunCount()) {
			return;
		}

		LOGTRACE("%s Selection Phase Target ID=%d", __PRETTY_FUNCTION__, GetTargetId())

		SetPhase(BUS::phase_t::selection);

		// Raise BSY and respond
		GetBus().SetBSY(true);
		return;
	}

	// Selection completed
	if (!GetBus().GetSEL() && GetBus().GetBSY()) {
		// Message out phase if ATN=1, otherwise command phase
		if (GetBus().GetATN()) {
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

		GetBus().SetMSG(false);
		GetBus().SetCD(true);
		GetBus().SetIO(false);

		const int actual_count = GetBus().CommandHandShake(GetBuffer());
		if (actual_count == 0) {
			LOGTRACE("ID %d LUN %d received unknown command: $%02X", GetTargetId(), GetEffectiveLun(), GetBuffer()[0])

			Error(sense_key::ILLEGAL_REQUEST, asc::INVALID_COMMAND_OPERATION_CODE);
			return;
		}

		const int command_byte_count = BUS::GetCommandByteCount(GetBuffer()[0]);

		// If not able to receive all, move to the status phase
		if (actual_count != command_byte_count) {
			LOGERROR("Command byte count mismatch for command $%02X: expected %d bytes, received %d byte(s)",
					GetBuffer()[0], command_byte_count, actual_count)
			Error(sense_key::ABORTED_COMMAND);
			return;
		}

		AllocateCmd(command_byte_count);

		// Command data transfer
		stringstream s;
		for (int i = 0; i < command_byte_count; i++) {
			GetCmd()[i] = GetBuffer()[i];
			s << setfill('0') << setw(2) << hex << GetCmd(i);
		}
		LOGTRACE("%s CDB=$%s",__PRETTY_FUNCTION__, s.str().c_str())

		SetLength(0);

		Execute();
	}
}

void ScsiController::Execute()
{
	LOGTRACE("%s ++++ CMD ++++ Executing command $%02X", __PRETTY_FUNCTION__, static_cast<int>(GetOpcode()))

	// Initialization for data transfer
	ResetOffset();
	SetBlocks(1);
	execstart = SysTimer::GetTimerLow();

	// Discard pending sense data from the previous command if the current command is not REQUEST SENSE
	if (GetOpcode() != scsi_command::eCmdRequestSense) {
		SetStatus(status::GOOD);
	}

	int lun = GetEffectiveLun();
	if (!HasDeviceForLun(lun)) {
		if (GetOpcode() != scsi_command::eCmdInquiry && GetOpcode() != scsi_command::eCmdRequestSense) {
			LOGTRACE("Invalid LUN %d for device ID %d", lun, GetTargetId())

			Error(sense_key::ILLEGAL_REQUEST, asc::INVALID_LUN);

			return;
		}

		assert(HasDeviceForLun(0));

		lun = 0;
	}

	// SCSI-2 4.4.3 Incorrect logical unit handling
	if (GetOpcode() == scsi_command::eCmdInquiry && !HasDeviceForLun(lun)) {
		LOGTRACE("Reporting LUN %d for device ID %d as not supported", GetEffectiveLun(), GetTargetId())

		GetBuffer().data()[0] = 0x7f;

		return;
	}

	auto device = GetDeviceForLun(lun);

	// Discard pending sense data from the previous command if the current command is not REQUEST SENSE
	if (GetOpcode() != scsi_command::eCmdRequestSense) {
		device->SetStatusCode(0);
	}

	if (device->CheckReservation(initiator_id, GetOpcode(), GetCmd(4) & 0x01)) {
		try {
			device->Dispatch(GetOpcode());
		}
		catch(const scsi_exception& e) {
			Error(e.get_sense_key(), e.get_asc());
		}
	}
	else {
		Error(sense_key::ABORTED_COMMAND, asc::NO_ADDITIONAL_SENSE_INFORMATION, status::RESERVATION_CONFLICT);
	}
}

void ScsiController::Status()
{
	if (!IsStatus()) {
		// Minimum execution time
		if (execstart > 0) {
			Sleep();
		} else {
			SysTimer::SleepUsec(5);
		}

		LOGTRACE("%s Status Phase, status is $%02X",__PRETTY_FUNCTION__, static_cast<int>(GetStatus()))

		SetPhase(BUS::phase_t::status);

		// Signal line operated by the target
		GetBus().SetMSG(false);
		GetBus().SetCD(true);
		GetBus().SetIO(true);

		// Data transfer is 1 byte x 1 block
		ResetOffset();
		SetLength(1);
		SetBlocks(1);
		GetBuffer()[0] = (uint8_t)GetStatus();

		return;
	}

	Send();
}

void ScsiController::MsgIn()
{
	if (!IsMsgIn()) {
		LOGTRACE("%s Message In phase", __PRETTY_FUNCTION__)

		SetPhase(BUS::phase_t::msgin);

		GetBus().SetMSG(true);
		GetBus().SetCD(true);
		GetBus().SetIO(true);

		ResetOffset();
		return;
	}

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

		GetBus().SetMSG(true);
		GetBus().SetCD(true);
		GetBus().SetIO(false);

		// Data transfer is 1 byte x 1 block
		ResetOffset();
		SetLength(1);
		SetBlocks(1);

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

		GetBus().SetMSG(false);
		GetBus().SetCD(false);
		GetBus().SetIO(true);

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
		GetBus().SetMSG(false);
		GetBus().SetCD(false);
		GetBus().SetIO(false);

		ResetOffset();
		return;
	}

	Receive();
}

void ScsiController::Error(sense_key sense_key, asc asc, status status)
{
	// Get bus information
	GetBus().Acquire();

	// Reset check
	if (GetBus().GetRST()) {
		Reset();
		GetBus().Reset();

		return;
	}

	// Bus free for status phase and message in phase
	if (IsStatus() || IsMsgIn()) {
		BusFree();
		return;
	}

	int lun = GetEffectiveLun();
	if (!HasDeviceForLun(lun) || asc == asc::INVALID_LUN) {
		if (!HasDeviceForLun(0)) {
			LOGERROR("No LUN 0 for device %d", GetTargetId())

			SetStatus(status);
			SetMessage(0x00);

			Status();

			return;
		}

		lun = 0;
	}

	if (sense_key != sense_key::NO_SENSE || asc != asc::NO_ADDITIONAL_SENSE_INFORMATION) {
		LOGDEBUG("Error status: Sense Key $%02X, ASC $%02X", static_cast<int>(sense_key), static_cast<int>(asc))

		// Set Sense Key and ASC for a subsequent REQUEST SENSE
		GetDeviceForLun(lun)->SetStatusCode((static_cast<int>(sense_key) << 16) | (static_cast<int>(asc) << 8));
	}

	SetStatus(status);
	SetMessage(0x00);

	LOGTRACE("%s Error (to status phase)", __PRETTY_FUNCTION__)

	Status();
}

void ScsiController::Send()
{
	assert(!GetBus().GetREQ());
	assert(GetBus().GetIO());

	if (HasValidLength()) {
		LOGTRACE("%s%s", __PRETTY_FUNCTION__, (" Sending handhake with offset " + to_string(GetOffset()) + ", length "
				+ to_string(GetLength())).c_str())

		// The delay should be taken from the respective LUN, but as there are no Daynaport drivers for
		// LUNs other than 0 this work-around works.
		if (const int len = GetBus().SendHandShake(GetBuffer().data() + GetOffset(), GetLength(),
				HasDeviceForLun(0) ? GetDeviceForLun(0)->GetSendDelay() : 0);
			len != static_cast<int>(GetLength())) {
			// If you cannot send all, move to status phase
			Error(sense_key::ABORTED_COMMAND);
			return;
		}

		UpdateOffsetAndLength();

		return;
	}

	// Block subtraction, result initialization
	DecrementBlocks();
	bool result = true;

	// Processing after data collection (read/data-in only)
	if (IsDataIn() && GetBlocks() != 0) {
		// set next buffer (set offset, length)
		result = XferIn(GetBuffer());
		LOGTRACE("%s%s", __PRETTY_FUNCTION__, (" Processing after data collection. Blocks: " + to_string(GetBlocks())).c_str())
	}

	// If result FALSE, move to status phase
	if (!result) {
		Error(sense_key::ABORTED_COMMAND);
		return;
	}

	// Continue sending if block !=0
	if (GetBlocks() != 0){
		LOGTRACE("%s%s", __PRETTY_FUNCTION__, (" Continuing to send. Blocks: " + to_string(GetBlocks())).c_str())
		assert(HasValidLength());
		assert(GetOffset() == 0);
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
			SetLength(1);
			SetBlocks(1);
			GetBuffer()[0] = (uint8_t)GetMessage();
			MsgIn();
			break;

		default:
			assert(false);
			break;
	}
}

void ScsiController::Receive()
{
	if (IsByteTransfer()) {
		ReceiveBytes();
		return;
	}

	LOGTRACE("%s",__PRETTY_FUNCTION__)

	// REQ is low
	assert(!GetBus().GetREQ());
	assert(!GetBus().GetIO());

	// Length != 0 if received
	if (HasValidLength()) {
		LOGTRACE("%s Length is %d byte(s)", __PRETTY_FUNCTION__, GetLength())

		// If not able to receive all, move to status phase
		if (int len = GetBus().ReceiveHandShake(GetBuffer().data() + GetOffset(), GetLength());
			len != static_cast<int>(GetLength())) {
			LOGERROR("%s Not able to receive %d byte(s) of data, only received %d",__PRETTY_FUNCTION__, GetLength(), len)
			Error(sense_key::ABORTED_COMMAND);
			return;
		}

		UpdateOffsetAndLength();

		return;
	}

	// Block subtraction, result initialization
	DecrementBlocks();
	bool result = true;

	// Processing after receiving data (by phase)
	LOGTRACE("%s Phase: %s",__PRETTY_FUNCTION__, BUS::GetPhaseStrRaw(GetPhase()))
	switch (GetPhase()) {
		case BUS::phase_t::dataout:
			if (GetBlocks() == 0) {
				// End with this buffer
				result = XferOut(false);
			} else {
				// Continue to next buffer (set offset, length)
				result = XferOut(true);
			}
			break;

		case BUS::phase_t::msgout:
			SetMessage(GetBuffer()[0]);
			if (!XferMsg(GetMessage())) {
				// Immediately free the bus if message output fails
				BusFree();
				return;
			}

			// Clear message data in preparation for message-in
			SetMessage(0x00);
			break;

		default:
			break;
	}

	// If result FALSE, move to status phase
	if (!result) {
		Error(sense_key::ABORTED_COMMAND);
		return;
	}

	// Continue to receive if block != 0
	if (GetBlocks() != 0) {
		assert(HasValidLength());
		assert(GetOffset() == 0);
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
		scsi.msb[scsi.msc] = (uint8_t)msg;
		scsi.msc++;
		scsi.msc %= 256;
	}

	return true;
}

void ScsiController::ReceiveBytes()
{
	assert(!GetBus().GetREQ());
	assert(!GetBus().GetIO());

	if (HasValidLength()) {
		LOGTRACE("%s Length is %d bytes", __PRETTY_FUNCTION__, GetLength())

		// If not able to receive all, move to status phase
		if (uint32_t len = GetBus().ReceiveHandShake(GetBuffer().data() + GetOffset(), GetLength()); len != GetLength()) {
			LOGERROR("%s Not able to receive %d byte(s) of data, only received %d",
					__PRETTY_FUNCTION__, GetLength(), len)
			Error(sense_key::ABORTED_COMMAND);
			return;
		}

		SetBytesToTransfer(GetLength());

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
			SetMessage(GetBuffer()[0]);
			if (!XferMsg(GetMessage())) {
				// Immediately free the bus if message output fails
				BusFree();
				return;
			}

			// Clear message data in preparation for message-in
			SetMessage(0x00);
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

	if (!IsByteTransfer()) {
		return XferOutBlockOriented(cont);
	}

	const uint32_t count = GetBytesToTransfer();
	SetByteTransfer(false);

	auto device = GetDeviceForLun(GetEffectiveLun());
	return device != nullptr ? device->WriteByteSequence(GetBuffer(), count) : false;
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
				if (auto device = dynamic_pointer_cast<ModePageDevice>(GetDeviceForLun(GetEffectiveLun()));
					device != nullptr) {
					device->ModeSelect(GetOpcode(), GetCmd(), GetBuffer(), GetOffset());
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
			LOGWARN("Unexpected Data Out phase for command $%02X", static_cast<int>(GetOpcode()))
			break;
	}
}

//---------------------------------------------------------------------------
//
//	Data transfer IN
//	*Reset offset and length
//
//---------------------------------------------------------------------------
bool ScsiController::XferIn(vector<uint8_t>& buf)
{
	assert(IsDataIn());

	LOGTRACE("%s command=%02X", __PRETTY_FUNCTION__, static_cast<int>(GetOpcode()))

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
				SetLength(dynamic_pointer_cast<Disk>(GetDeviceForLun(lun))->Read(GetCmd(), buf, GetNext()));
			}
			catch(const scsi_exception&) {
				// If there is an error, go to the status phase
				return false;
			}

			IncrementNext();

			// If things are normal, work setting
			ResetOffset();
			break;

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
// TODO Try to use less casts
//
//---------------------------------------------------------------------------
bool ScsiController::XferOutBlockOriented(bool cont)
{
	auto device = GetDeviceForLun(GetEffectiveLun());

	// Limited to write commands
	switch (GetOpcode()) {
		case scsi_command::eCmdModeSelect6:
		case scsi_command::eCmdModeSelect10:
		{
			auto mode_page_device = dynamic_pointer_cast<ModePageDevice>(device);
			if (mode_page_device == nullptr) {
				return false;
			}

			try {
				mode_page_device->ModeSelect(GetOpcode(), GetCmd(), GetBuffer(), GetOffset());
			}
			catch(const scsi_exception& e) {
				Error(e.get_sense_key(), e.get_asc());
				return false;
			}

			break;
		}

		case scsi_command::eCmdWrite6:
		case scsi_command::eCmdWrite10:
		case scsi_command::eCmdWrite16:
		// TODO Verify has to verify, not to write
		case scsi_command::eCmdVerify10:
		case scsi_command::eCmdVerify16:
		{
			// Special case for SCBR and SCDP
			if (auto byte_writer = dynamic_pointer_cast<ByteWriter>(device); byte_writer) {
				if (!byte_writer->WriteBytes(GetCmd(), GetBuffer(), GetLength())) {
					return false;
				}

				ResetOffset();
				break;
			}

			auto disk = dynamic_pointer_cast<Disk>(device);
			if (disk == nullptr) {
				return false;
			}

			try {
				disk->Write(GetCmd(), GetBuffer(), GetNext() - 1);
			}
			catch(const scsi_exception& e) {
				Error(e.get_sense_key(), e.get_asc());

				return false;
			}

			// If you do not need the next block, end here
			IncrementNext();
			if (!cont) {
				break;
			}

			SetLength(disk->GetSectorSizeInBytes());
			ResetOffset();
			break;
		}

		case scsi_command::eCmdSetMcastAddr:
			LOGTRACE("%s Done with DaynaPort Set Multicast Address", __PRETTY_FUNCTION__)
			break;

		default:
			LOGWARN("Received an unexpected command ($%02X) in %s", static_cast<int>(GetOpcode()), __PRETTY_FUNCTION__)
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
		GetCmd()[i] = GetBuffer()[i];
		s << GetCmd(i);
	}
	LOGTRACE("%s CDB=$%s",__PRETTY_FUNCTION__, s.str().c_str())

	Execute();
}

void ScsiController::ParseMessage()
{
	int i = 0;
	while (i < scsi.msc) {
		const uint8_t message_type = scsi.msb[i];

		if (message_type == 0x06) {
			LOGTRACE("Received ABORT message")
			BusFree();
			return;
		}

		if (message_type == 0x0C) {
			LOGTRACE("Received BUS DEVICE RESET message")
			scsi.syncoffset = 0;
			if (auto device = GetDeviceForLun(identified_lun); device != nullptr) {
				device->DiscardReservation();
			}
			BusFree();
			return;
		}

		if (message_type >= 0x80) {
			identified_lun = static_cast<int>(message_type) & 0x1F;
			LOGTRACE("Received IDENTIFY message for LUN %d", identified_lun)
		}

		if (message_type == 0x01) {
			LOGTRACE("Received EXTENDED MESSAGE")

			// Check only when synchronous transfer is possible
			if (!scsi.syncenable || scsi.msb[i + 2] != 0x01) {
				SetLength(1);
				SetBlocks(1);
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
			SetLength(5);
			SetBlocks(1);
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
	if (GetBus().GetATN()) {
		// Data transfer is 1 byte x 1 block
		ResetOffset();
		SetLength(1);
		SetBlocks(1);
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

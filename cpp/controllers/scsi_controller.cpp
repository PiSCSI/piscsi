//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI
//	for Raspberry Pi
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//  Copyright (C) akuker
//  Copyright (C) 2022-2023 Uwe Seimet
//
//  	Licensed under the BSD 3-Clause License.
//  	See LICENSE file in the project root folder.
//
//---------------------------------------------------------------------------

#include "shared/piscsi_exceptions.h"
#include "hal/gpiobus.h"
#include "hal/systimer.h"
#include "controllers/controller_manager.h"
#include "devices/scsi_host_bridge.h"
#include "devices/scsi_daynaport.h"
#include "devices/mode_page_device.h"
#include "devices/disk.h"
#include "scsi_controller.h"
#include <sstream>
#include <iomanip>
#ifdef __linux__
#include <linux/if_tun.h>
#endif

using namespace scsi_defs;

ScsiController::ScsiController(BUS& bus, int target_id) : AbstractController(bus, target_id, ControllerManager::GetScsiLunMax())
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
	initiator_id = UNKNOWN_INITIATOR_ID;

	scsi = {};
}

bool ScsiController::Process(int id)
{
	GetBus().Acquire();

	if (GetBus().GetRST()) {
		LogWarn("RESET signal received!");

		Reset();

		return false;
	}

	initiator_id = id;

	try {
		ProcessPhase();
	}
	catch(const scsi_exception&) {
		LogError("Unhandled SCSI error, resetting controller and bus and entering bus free phase");

		Reset();

		BusFree();
	}

	return !IsBusFree();
}

void ScsiController::BusFree()
{
	if (!IsBusFree()) {
		LogTrace("Bus Free phase");
		SetPhase(phase_t::busfree);

		GetBus().SetREQ(false);
		GetBus().SetMSG(false);
		GetBus().SetCD(false);
		GetBus().SetIO(false);
		GetBus().SetBSY(false);

		// Initialize status and message
		SetStatus(status::good);
		SetMessage(0x00);

		// Initialize ATN message reception status
		scsi.atnmsg = false;

		identified_lun = -1;

		SetByteTransfer(false);

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
		LogTrace("Selection phase");
		SetPhase(phase_t::selection);

		// Raise BSY and respond
		GetBus().SetBSY(true);
		return;
	}

	// Selection completed
	if (!GetBus().GetSEL() && GetBus().GetBSY()) {
		LogTrace("Selection completed");

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
		LogTrace("Command phase");
		SetPhase(phase_t::command);

		GetBus().SetMSG(false);
		GetBus().SetCD(true);
		GetBus().SetIO(false);

		const int actual_count = GetBus().CommandHandShake(GetBuffer());
		if (actual_count == 0) {
			stringstream s;
			s << "Received unknown command: $" << setfill('0') << setw(2) << hex << GetBuffer()[0];
			LogTrace(s.str());

			Error(sense_key::illegal_request, asc::invalid_command_operation_code);
			return;
		}

		const int command_byte_count = BUS::GetCommandByteCount(GetBuffer()[0]);

		// If not able to receive all, move to the status phase
		if (actual_count != command_byte_count) {
			stringstream s;
			s << "Command byte count mismatch for command $" << setfill('0') << setw(2) << hex << GetBuffer()[0];
			LogError(s.str() + ": expected " + to_string(command_byte_count) + " bytes, received"
					+ to_string(actual_count) + " byte(s)");
			Error(sense_key::aborted_command);
			return;
		}

		// Command data transfer
		AllocateCmd(command_byte_count);
		for (int i = 0; i < command_byte_count; i++) {
			SetCmdByte(i, GetBuffer()[i]);
		}

		SetLength(0);

		Execute();
	}
}

void ScsiController::Execute()
{
    if (spdlog::get_level() == spdlog::level::trace) {
        stringstream s;
        s << "Controller is executing " << command_mapping.find(GetOpcode())->second.second << ", CDB $"
            << setfill('0') << hex;
        for (int i = 0; i < BUS::GetCommandByteCount(static_cast<uint8_t>(GetOpcode())); i++) {
            s << setw(2) << GetCmdByte(i);
        }
        LogTrace(s.str());
    }

	// Initialization for data transfer
	ResetOffset();
	SetBlocks(1);
	execstart = SysTimer::GetTimerLow();

	// Discard pending sense data from the previous command if the current command is not REQUEST SENSE
	if (GetOpcode() != scsi_command::eCmdRequestSense) {
		SetStatus(status::good);
	}

	int lun = GetEffectiveLun();
	if (!HasDeviceForLun(lun)) {
		if (GetOpcode() != scsi_command::eCmdInquiry && GetOpcode() != scsi_command::eCmdRequestSense) {
			LogTrace("Invalid LUN " + to_string(lun));

			Error(sense_key::illegal_request, asc::invalid_lun);

			return;
		}

		assert(HasDeviceForLun(0));

		lun = 0;
	}

	// SCSI-2 4.4.3 Incorrect logical unit handling
	if (GetOpcode() == scsi_command::eCmdInquiry && !HasDeviceForLun(lun)) {
		LogTrace("Reporting LUN" + to_string(GetEffectiveLun()) + " as not supported");

		GetBuffer().data()[0] = 0x7f;

		return;
	}

	auto device = GetDeviceForLun(lun);

	// Discard pending sense data from the previous command if the current command is not REQUEST SENSE
	if (GetOpcode() != scsi_command::eCmdRequestSense) {
		device->SetStatusCode(0);
	}

	if (device->CheckReservation(initiator_id, GetOpcode(), GetCmdByte(4) & 0x01)) {
		try {
			device->Dispatch(GetOpcode());
		}
		catch(const scsi_exception& e) {
			Error(e.get_sense_key(), e.get_asc());
		}
	}
	else {
		Error(sense_key::aborted_command, asc::no_additional_sense_information, status::reservation_conflict);
	}
}

void ScsiController::Status()
{
	if (!IsStatus()) {
		// Minimum execution time
		// TODO Why is a delay needed? Is this covered by the SCSI specification?
		if (execstart > 0) {
			Sleep();
		} else {
			SysTimer::SleepUsec(5);
		}

		stringstream s;
		s << "Status phase, status is $" << setfill('0') << setw(2) << hex << static_cast<int>(GetStatus());
		LogTrace(s.str());
		SetPhase(phase_t::status);

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
		LogTrace("Message In phase");
		SetPhase(phase_t::msgin);

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
	if (!IsMsgOut()) {
	    // process the IDENTIFY message
		if (IsSelection()) {
			scsi.atnmsg = true;
			scsi.msc = 0;
			scsi.msb = {};
		}

		LogTrace("Message Out phase");
		SetPhase(phase_t::msgout);

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

		LogTrace("Data In phase");
		SetPhase(phase_t::datain);

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

		LogTrace("Data Out phase");
		SetPhase(phase_t::dataout);

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

		return;
	}

	// Bus free for status phase and message in phase
	if (IsStatus() || IsMsgIn()) {
		BusFree();
		return;
	}

	int lun = GetEffectiveLun();
	if (!HasDeviceForLun(lun) || asc == asc::invalid_lun) {
		if (!HasDeviceForLun(0)) {
			LogError("No LUN 0");

			SetStatus(status);
			SetMessage(0x00);

			Status();

			return;
		}

		lun = 0;
	}

	if (sense_key != sense_key::no_sense || asc != asc::no_additional_sense_information) {
		stringstream s;
		s << setfill('0') << hex << "Error status: Sense Key $" << setw(2) << static_cast<int>(sense_key)
				<< ", ASC $" << setw(2) << static_cast<int>(asc);
		LogDebug(s.str());

		// Set Sense Key and ASC for a subsequent REQUEST SENSE
		GetDeviceForLun(lun)->SetStatusCode((static_cast<int>(sense_key) << 16) | (static_cast<int>(asc) << 8));
	}

	SetStatus(status);
	SetMessage(0x00);

	Status();
}

void ScsiController::Send()
{
	assert(!GetBus().GetREQ());
	assert(GetBus().GetIO());

	if (HasValidLength()) {
		LogTrace("Sending data, offset: " + to_string(GetOffset()) + ", length: " + to_string(GetLength()));

		// The delay should be taken from the respective LUN, but as there are no Daynaport drivers for
		// LUNs other than 0 this work-around works.
		if (const int len = GetBus().SendHandShake(GetBuffer().data() + GetOffset(), GetLength(),
				HasDeviceForLun(0) ? GetDeviceForLun(0)->GetSendDelay() : 0);
			len != static_cast<int>(GetLength())) {
			// If you cannot send all, move to status phase
			Error(sense_key::aborted_command);
			return;
		}

		UpdateOffsetAndLength();

		return;
	}

	DecrementBlocks();

	// Processing after data collection (read/data-in only)
	if (IsDataIn() && HasBlocks()) {
		// set next buffer (set offset, length)
		if (!XferIn(GetBuffer())) {
			// If result FALSE, move to status phase
			Error(sense_key::aborted_command);
			return;
		}

		LogTrace("Processing after data collection");
	}

	// Continue sending if blocks != 0
	if (HasBlocks()) {
		LogTrace("Continuing to send");
		assert(HasValidLength());
		assert(GetOffset() == 0);
		return;
	}

	// Move to next phase
	LogTrace("All data transferred, moving to next phase: " + string(BUS::GetPhaseStrRaw(GetPhase())));
	switch (GetPhase()) {
		case phase_t::msgin:
			// Completed sending response to extended message of IDENTIFY message
			if (scsi.atnmsg) {
				scsi.atnmsg = false;

				Command();
			} else {
				BusFree();
			}
			break;

		case phase_t::datain:
			Status();
			break;

		case phase_t::status:
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
	assert(!GetBus().GetREQ());
	assert(!GetBus().GetIO());

	if (HasValidLength()) {
		LogTrace("Receiving data, transfer length: " + to_string(GetLength()) + " byte(s)");

		// If not able to receive all, move to status phase
		if (uint32_t len = GetBus().ReceiveHandShake(GetBuffer().data() + GetOffset(), GetLength()); len != GetLength()) {
			LogError("Not able to receive " + to_string(GetLength()) + " byte(s) of data, only received "
					+ to_string(len));
			Error(sense_key::aborted_command);
			return;
		}
	}

	if (IsByteTransfer()) {
		ReceiveBytes();
		return;
	}

	if (HasValidLength()) {
		UpdateOffsetAndLength();
		return;
	}

	DecrementBlocks();
	bool result = true;

	// Processing after receiving data (by phase)
	LogTrace("Phase: " + string(BUS::GetPhaseStrRaw(GetPhase())));
	switch (GetPhase()) {
		case phase_t::dataout:
			if (!HasBlocks()) {
				// End with this buffer
				result = XferOut(false);
			} else {
				// Continue to next buffer (set offset, length)
				result = XferOut(true);
			}
			break;

		case phase_t::msgout:
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
		Error(sense_key::aborted_command);
		return;
	}

	// Continue to receive if blocks != 0
	if (HasBlocks()) {
		assert(HasValidLength());
		assert(GetOffset() == 0);
		return;
	}

	// Move to next phase
	switch (GetPhase()) {
		case phase_t::command:
			ProcessCommand();
			break;

		case phase_t::msgout:
			ProcessMessage();
			break;

		case phase_t::dataout:
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
	if (HasValidLength()) {
		SetBytesToTransfer(GetLength());
		UpdateOffsetAndLength();
		return;
	}

	bool result = true;

	// Processing after receiving data (by phase)
	LogTrace("Phase: " + string(BUS::GetPhaseStrRaw(GetPhase())));
	switch (GetPhase()) {
		case phase_t::dataout:
			result = XferOut(false);
			break;

		case phase_t::msgout:
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
		Error(sense_key::aborted_command);
		return;
	}

	// Move to next phase
	switch (GetPhase()) {
		case phase_t::command:
			ProcessCommand();
			break;

		case phase_t::msgout:
			ProcessMessage();
			break;

		case phase_t::dataout:
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
	return device != nullptr ? device->WriteByteSequence(span(GetBuffer().data(), count)) : false;
}

void ScsiController::DataOutNonBlockOriented() const
{
	assert(IsDataOut());

	switch (GetOpcode()) {
		case scsi_command::eCmdWrite6:
		case scsi_command::eCmdWrite10:
		case scsi_command::eCmdWrite16:
		case scsi_command::eCmdWriteLong10:
		case scsi_command::eCmdWriteLong16:
		case scsi_command::eCmdVerify10:
		case scsi_command::eCmdVerify16:
		case scsi_command::eCmdModeSelect6:
		case scsi_command::eCmdModeSelect10:
			break;

		case scsi_command::eCmdSetMcastAddr:
			// TODO: Eventually, we should store off the multicast address configuration data here...
			break;

		default:
			stringstream s;
			s << "Unexpected Data Out phase for command $" << setfill('0') << setw(2) << hex
					<< static_cast<int>(GetOpcode());
			LogWarn(s.str());
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

	stringstream s;
	s << "Command: $" << setfill('0') << setw(2) << hex << static_cast<int>(GetOpcode());
	LogTrace(s.str());

	const int lun = GetEffectiveLun();
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
				SetLength(dynamic_pointer_cast<Disk>(GetDeviceForLun(lun))->Read(buf, GetNext()));
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
		{
			// TODO Get rid of this special case for SCBR
			if (auto bridge = dynamic_pointer_cast<SCSIBR>(device); bridge) {
				if (!bridge->ReadWrite(GetCmd(), GetBuffer())) {
					return false;
				}

				ResetOffset();
				break;
			}

			// TODO Get rid of this special case for SCDP
			if (auto daynaport = dynamic_pointer_cast<SCSIDaynaPort>(device); daynaport) {
				if (!daynaport->Write(GetCmd(), GetBuffer())) {
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
				disk->Write(GetBuffer(), GetNext() - 1);
			}
			catch(const scsi_exception& e) {
				Error(e.get_sense_key(), e.get_asc());

				return false;
			}

			// If you do not need the next block, end here
			IncrementNext();
			if (cont) {
				SetLength(disk->GetSectorSizeInBytes());
				ResetOffset();
			}

			break;
		}

		case scsi_command::eCmdVerify10:
		case scsi_command::eCmdVerify16:
		{
			auto disk = dynamic_pointer_cast<Disk>(device);
			if (disk == nullptr) {
				return false;
			}

			// If you do not need the next block, end here
			IncrementNext();
			if (cont) {
				SetLength(disk->GetSectorSizeInBytes());
				ResetOffset();
			}

			break;
		}

		case scsi_command::eCmdSetMcastAddr:
			LogTrace("Done with DaynaPort Set Multicast Address");
			break;

		default:
			stringstream s;
			s << "Received an unexpected command ($" << setfill('0') << setw(2) << hex
					<< static_cast<int>(GetOpcode()) << ")";
			LogWarn(s.str());
			break;
	}

	return true;
}

void ScsiController::ProcessCommand()
{
	const uint32_t len = GPIOBUS::GetCommandByteCount(GetBuffer()[0]);

	stringstream s;
	s << "CDB=$" << setfill('0') << setw(2) << hex;
	for (uint32_t i = 0; i < len; i++) {
		SetCmdByte(i, GetBuffer()[i]);
		s << GetCmdByte(i);
	}
	LogTrace(s.str());

	Execute();
}

void ScsiController::ParseMessage()
{
	int i = 0;
	while (i < scsi.msc) {
		const uint8_t message_type = scsi.msb[i];

		if (message_type == 0x06) {
			LogTrace("Received ABORT message");
			BusFree();
			return;
		}

		if (message_type == 0x0C) {
			LogTrace("Received BUS DEVICE RESET message");
			scsi.syncoffset = 0;
			if (auto device = GetDeviceForLun(identified_lun); device != nullptr) {
				device->DiscardReservation();
			}
			BusFree();
			return;
		}

		if (message_type >= 0x80) {
			identified_lun = static_cast<int>(message_type) & 0x1F;
			LogTrace("Received IDENTIFY message for LUN " + to_string(identified_lun));
		}

		if (message_type == 0x01) {
			LogTrace("Received EXTENDED MESSAGE");

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

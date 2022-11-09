//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "shared/log.h"
#include "shared/rascsi_exceptions.h"
#include "devices/primary_device.h"
#include "abstract_controller.h"

void AbstractController::AllocateCmd(size_t size)
{
	if (size > ctrl.cmd.size()) {
		ctrl.cmd.resize(size);
	}
}

void AbstractController::AllocateBuffer(size_t size)
{
	if (size > ctrl.buffer.size()) {
		ctrl.buffer.resize(size);
	}
}

void AbstractController::SetByteTransfer(bool b)
{
	 is_byte_transfer = b;

	 if (!is_byte_transfer) {
		 bytes_to_transfer = 0;
	 }
}

unordered_set<shared_ptr<PrimaryDevice>> AbstractController::GetDevices() const
{
	unordered_set<shared_ptr<PrimaryDevice>> devices;

	for (const auto& [id, lun] : luns) {
		devices.insert(lun);
	}

	return devices;
}

shared_ptr<PrimaryDevice> AbstractController::GetDeviceForLun(int lun) const {
	const auto& it = luns.find(lun);
	return it == luns.end() ? nullptr : it->second;
}

void AbstractController::Reset()
{
	SetPhase(BUS::phase_t::busfree);

	ctrl.status = status::GOOD;
	ctrl.message = 0x00;
	ctrl.blocks = 0;
	ctrl.next = 0;
	ctrl.offset = 0;
	ctrl.length = 0;

	// Reset all LUNs
	for (const auto& [lun, device] : luns) {
		device->Reset();
	}
}

void AbstractController::ProcessPhase()
{
	switch (GetPhase()) {
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
			LOGERROR("Cannot process phase %s", BUS::GetPhaseStrRaw(GetPhase()))
			throw scsi_exception(sense_key::ABORTED_COMMAND);
			break;
	}
}

bool AbstractController::AddDevice(shared_ptr<PrimaryDevice> device)
{
	if (device->GetLun() < 0 || device->GetLun() >= GetMaxLuns() || HasDeviceForLun(device->GetLun())) {
		return false;
	}

	luns[device->GetLun()] = device;
	device->SetController(shared_from_this());

	return true;
}

bool AbstractController::RemoveDevice(shared_ptr<PrimaryDevice> device)
{
	device->SetController(nullptr);

	return luns.erase(device->GetLun()) == 1;
}

bool AbstractController::HasDeviceForLun(int lun) const
{
	return luns.find(lun) != luns.end();
}

int AbstractController::ExtractInitiatorId(int id_data) const
{
	int initiator_id = -1;

	if (int tmp = id_data - (1 << target_id); tmp) {
		initiator_id = 0;
		for (int j = 0; j < 8; j++) {
			tmp >>= 1;
			if (tmp) {
				initiator_id++;
			}
			else {
				break;
			}
		}
	}

	return initiator_id;
}

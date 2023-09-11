//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "shared/piscsi_exceptions.h"
#include "devices/primary_device.h"
#include "abstract_controller.h"
#include <ranges>

using namespace scsi_defs;

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

	// "luns | views:values" is not supported by the bullseye compiler
	ranges::transform(luns, inserter(devices, devices.begin()), [] (const auto& l) { return l.second; } );

	return devices;
}

shared_ptr<PrimaryDevice> AbstractController::GetDeviceForLun(int lun) const {
	const auto& it = luns.find(lun);
	return it == luns.end() ? nullptr : it->second;
}

void AbstractController::Reset()
{
	SetPhase(phase_t::busfree);

	ctrl.status = status::good;
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
		case phase_t::busfree:
			BusFree();
			break;

		case phase_t::selection:
			Selection();
			break;

		case phase_t::dataout:
			DataOut();
			break;

		case phase_t::datain:
			DataIn();
			break;

		case phase_t::command:
			Command();
			break;

		case phase_t::status:
			Status();
			break;

		case phase_t::msgout:
			MsgOut();
			break;

		case phase_t::msgin:
			MsgIn();
			break;

		default:
			throw scsi_exception(sense_key::aborted_command);
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
	if (luns.erase(device->GetLun()) == 1) {
		device->SetController(nullptr);

		return true;
	}

	return false;
}

bool AbstractController::HasDeviceForLun(int lun) const
{
	return luns.contains(lun);
}

int AbstractController::ExtractInitiatorId(int id_data) const
{
	if (const int id_data_without_target = id_data - (1 << target_id); id_data_without_target) {
		return static_cast<int>(log2(id_data_without_target & -id_data_without_target));
	}

	return UNKNOWN_INITIATOR_ID;
}

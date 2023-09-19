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

	ctrl = {};

	// Reset all LUNs
	for (const auto& [_, device] : luns) {
		device->Reset();
	}
}

void AbstractController::ProcessOnController(int id_data)
{
	device_logger.SetIdAndLun(GetTargetId(), -1);

	const int initiator_id = ExtractInitiatorId(id_data);
	if (initiator_id != UNKNOWN_INITIATOR_ID) {
		device_logger.Trace("++++ Starting processing for initiator ID " + to_string(initiator_id));
	}
	else {
		device_logger.Trace("++++ Starting processing for unknown initiator ID");
	}

	while (Process(initiator_id) != phase_t::busfree) {
		// Handle bus phases until the bus is free
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

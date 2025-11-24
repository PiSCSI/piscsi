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

AbstractController::AbstractController(BUS& bus, int target_id, int max_luns) : bus(bus), target_id(target_id),
	max_luns(max_luns)
{
	device_logger.SetIdAndLun(target_id, -1);
}

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
	ranges::transform(luns, inserter(devices, devices.begin()), [] (const auto & l) { return l.second; } );

	return devices;
}

shared_ptr<PrimaryDevice> AbstractController::GetDeviceForLun(int lun) const
{
	const auto& it = luns.find(lun);
	return it == luns.end() ? nullptr : it->second;
}

void AbstractController::Reset()
{
	SetPhase(phase_t::busfree);

	ctrl = {};

	SetByteTransfer(false);

	// Reset all LUNs
	for (const auto& [_, device] : luns) {
		device->Reset();
	}

	GetBus().Reset();
}

void AbstractController::ProcessOnController(int id_data)
{
	device_logger.SetIdAndLun(GetTargetId(), -1);

	const int initiator_id = ExtractInitiatorId(id_data);

	if (initiator_id != UNKNOWN_INITIATOR_ID) {
		LogTrace("++++ Starting processing for initiator ID " + to_string(initiator_id));
	}
	else {
		LogTrace("++++ Starting processing for unknown initiator ID");
	}

	while (Process(initiator_id)) {
		// Handle bus phases until the bus is free for the next command
	}
}

bool AbstractController::AddDevice(shared_ptr<PrimaryDevice> device)
{
	const int lun = device->GetLun();

	if (lun < 0 || lun >= GetMaxLuns() || HasDeviceForLun(lun) || device->GetController()) {
		return false;
	}

	luns[lun] = device;
	device->SetController(this);

	return true;
}

bool AbstractController::RemoveDevice(PrimaryDevice& device)
{
	device.CleanUp();

	return luns.erase(device.GetLun()) == 1;
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

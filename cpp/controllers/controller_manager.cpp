//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "devices/primary_device.h"
#include "scsi_controller.h"
#include "controller_manager.h"

using namespace std;

bool ControllerManager::AttachToScsiController(int id, shared_ptr<PrimaryDevice> device)
{
	if (auto controller = FindController(id); controller != nullptr) {
		return controller->AddDevice(device);
	}

	// If this is LUN 0 create a new controller
	if (device->GetLun() == 0) {
		if (auto controller = make_shared<ScsiController>(shared_from_this(), id); controller->AddDevice(device)) {
			controllers[id] = controller;

			return true;
		}
	}

	return false;
}

bool ControllerManager::DeleteController(shared_ptr<AbstractController> controller)
{
	return controllers.erase(controller->GetTargetId()) == 1;
}

void ControllerManager::ProcessOnController(int id_data)
{
	const auto& it = ranges::find_if(controllers, [&] (const auto& c) { return (id_data & (1 << c.first)); } );
	if (it != controllers.end()) {
		AbstractController& controller = *(*it).second;

		device_logger.SetIdAndLun(controller.GetTargetId(), -1);

		const int initiator_id = controller.ExtractInitiatorId(id_data);
		if (initiator_id != AbstractController::UNKNOWN_INITIATOR_ID) {
			device_logger.Trace("++++ Starting processing for initiator ID " + to_string(initiator_id));
		}
		else {
			device_logger.Trace("++++ Starting processing for unknown initiator ID");
		}

		while (controller.Process(initiator_id) != phase_t::busfree) {
			// Handle bus phases until the bus is free
		}
	}
}

shared_ptr<AbstractController> ControllerManager::FindController(int target_id) const
{
	const auto& it = controllers.find(target_id);
	return it == controllers.end() ? nullptr : it->second;
}

bool ControllerManager::HasController(int target_id) const {
	return FindController(target_id) != nullptr;
}

unordered_set<shared_ptr<PrimaryDevice>> ControllerManager::GetAllDevices() const
{
	unordered_set<shared_ptr<PrimaryDevice>> devices;

	for (const auto& [_, controller] : controllers) {
		const auto& d = controller->GetDevices();
		devices.insert(d.begin(), d.end());
	}

	return devices;
}

void ControllerManager::DeleteAllControllers()
{
	controllers.clear();
}

shared_ptr<PrimaryDevice> ControllerManager::GetDeviceByIdAndLun(int id, int lun) const
{
	if (const auto& controller = FindController(id); controller != nullptr) {
		return controller->GetDeviceForLun(lun);
	}

	return nullptr;
}

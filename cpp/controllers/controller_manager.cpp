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

bool ControllerManager::AttachToController(int id, shared_ptr<PrimaryDevice> device)
{
	if (auto controller = FindController(id); controller != nullptr) {
		return controller->AddDevice(device);
	}

	// If this is LUN 0 create a new controller
	if (device->GetLun() == 0) {
		if (auto controller = make_shared<ScsiController>(bus, id); controller->AddDevice(device)) {
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

AbstractController::piscsi_shutdown_mode ControllerManager::ProcessOnController(int id_data) const
{
	const auto& it = ranges::find_if(controllers, [&] (const auto& c) { return (id_data & (1 << c.first)); } );
	if (it != controllers.end()) {
		(*it).second->ProcessOnController(id_data);

		return (*it).second->GetShutdownMode();
	}

	return AbstractController::piscsi_shutdown_mode::NONE;
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

void ControllerManager::FlushCaches() const
{
	for (const auto& device : GetAllDevices()) {
		device->FlushCache();
	}
}

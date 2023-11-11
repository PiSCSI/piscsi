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

shared_ptr<ScsiController> ControllerManager::CreateScsiController(BUS& bus, int id) const
{
	auto controller = make_shared<ScsiController>(bus, id);
	controller->Init();

	return controller;
}

bool ControllerManager::AttachToController(BUS& bus, int id, shared_ptr<PrimaryDevice> device)
{
	if (auto controller = FindController(id); controller != nullptr) {
		if (controller->HasDeviceForLun(device->GetLun())) {
			return false;
		}

		return controller->AddDevice(device);
	}

	// If this is LUN 0 create a new controller
	if (!device->GetLun()) {
		if (auto controller = CreateScsiController(bus, id); controller->AddDevice(device)) {
			controllers[id] = controller;

			return true;
		}
	}

	return false;
}

bool ControllerManager::DeleteController(const AbstractController& controller)
{
	for (const auto& device : controller.GetDevices()) {
		device->CleanUp();
	}

	return controllers.erase(controller.GetTargetId()) == 1;
}

void ControllerManager::DeleteAllControllers()
{
	unordered_set<shared_ptr<AbstractController>> values;
	ranges::transform(controllers, inserter(values, values.begin()), [] (const auto& controller) { return controller.second; } );

	for (const auto& controller : values) {
		DeleteController(*controller);
	}

	assert(controllers.empty());
}

AbstractController::piscsi_shutdown_mode ControllerManager::ProcessOnController(int id_data) const
{
	if (const auto& it = ranges::find_if(controllers, [&] (const auto& c) { return (id_data & (1 << c.first)); } );
		it != controllers.end()) {
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
	return controllers.contains(target_id);
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

bool ControllerManager::HasDeviceForIdAndLun(int id, int lun) const
{
	return GetDeviceForIdAndLun(id, lun) != nullptr;
}

shared_ptr<PrimaryDevice> ControllerManager::GetDeviceForIdAndLun(int id, int lun) const
{
	if (const auto& controller = FindController(id); controller != nullptr) {
		return controller->GetDeviceForLun(lun);
	}

	return nullptr;
}

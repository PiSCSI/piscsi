//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "devices/device_factory.h"
#include "devices/primary_device.h"
#include "scsi_controller.h"
#include "controller_manager.h"

using namespace std;

bool ControllerManager::AttachToScsiController(int id, shared_ptr<PrimaryDevice> device)
{
	auto controller = FindController(id);
	if (controller == nullptr) {
		controller = make_shared<ScsiController>(bus, id);
		if (controller->AddDevice(device)) {
			controllers[id] = controller;

			return true;
		}

		return false;
	}

	return controller->AddDevice(device);
}

bool ControllerManager::DeleteController(shared_ptr<AbstractController> controller)
{
	return controllers.erase(controller->GetTargetId()) == 1;
}

shared_ptr<AbstractController> ControllerManager::IdentifyController(int data) const
{
	for (const auto& [id, controller] : controllers) {
		if (data & (1 << controller->GetTargetId())) {
			return controller;
		}
	}

	return nullptr;
}

shared_ptr<AbstractController> ControllerManager::FindController(int target_id) const
{
	const auto& it = controllers.find(target_id);
	return it == controllers.end() ? nullptr : it->second;
}

unordered_set<shared_ptr<PrimaryDevice>> ControllerManager::GetAllDevices() const
{
	unordered_set<shared_ptr<PrimaryDevice>> devices;

	for (const auto& [id, controller] : controllers) {
		auto d = controller->GetDevices();
		devices.insert(d.begin(), d.end());
	}

	return devices;
}

void ControllerManager::DeleteAllControllers()
{
	controllers.clear();
}

void ControllerManager::ResetAllControllers() const
{
	for (const auto& [id, controller] : controllers) {
		controller->Reset();
	}
}

shared_ptr<PrimaryDevice> ControllerManager::GetDeviceByIdAndLun(int id, int lun) const
{
	if (const auto controller = FindController(id); controller != nullptr) {
		return controller->GetDeviceForLun(lun);
	}

	return nullptr;
}

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
#include "devices/file_support.h"
#include "scsi_controller.h"
#include "controller_manager.h"

using namespace std;

bool ControllerManager::CreateScsiController(int id, PrimaryDevice *device)
{
	assert(id == device->GetId());

	shared_ptr<AbstractController> controller = FindController(id);
	if (controller == nullptr) {
		controller = make_shared<ScsiController>(bus, id);
		controllers[id] = controller;
	}

	return controller->AddDevice(device);
}

bool ControllerManager::DeleteController(int target_id)
{
	if (auto controller = FindController(target_id); controller == nullptr || controller->GetLunCount() > 0) {
		return false;
	}

	controllers.erase(target_id);

	return true;
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

PrimaryDevice *ControllerManager::GetDeviceByIdAndLun(int id, int lun) const
{
	if (const auto controller = FindController(id); controller != nullptr) {
		return controller->GetDeviceForLun(lun);
	}

	return nullptr;
}

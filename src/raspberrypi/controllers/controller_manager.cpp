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

bool ControllerManager::CreateScsiController(BUS& bus, PrimaryDevice *device)
{
	shared_ptr<AbstractController> controller = FindController(device->GetId());
	if (controller == nullptr) {
		controller = make_shared<ScsiController>(bus, device->GetId());
		controllers[device->GetId()] = controller;
	}

	return controller->AddDevice(device);
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

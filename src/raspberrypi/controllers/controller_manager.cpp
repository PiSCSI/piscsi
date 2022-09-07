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

unordered_map<int, AbstractController *> ControllerManager::controllers;

ControllerManager::~ControllerManager()
{
	DeleteAllControllersAndDevices();
}

ControllerManager& ControllerManager::instance()
{
	static ControllerManager instance;
	return instance;
}

bool ControllerManager::CreateScsiController(BUS *bus, PrimaryDevice *device)
{
	AbstractController *controller = FindController(device->GetId());
	if (controller == nullptr) {
		controller = new ScsiController(bus, device->GetId());
		controllers[device->GetId()] = controller;
	}

	return controller->AddDevice(device);
}

AbstractController *ControllerManager::IdentifyController(int data) const
{
	for (const auto& [id, controller] : controllers) {
		if (data & (1 << controller->GetTargetId())) {
			return controller;
		}
	}

	return nullptr;
}

AbstractController *ControllerManager::FindController(int target_id) const
{
	const auto& it = controllers.find(target_id);
	return it == controllers.end() ? nullptr : it->second;
}

void ControllerManager::DeleteAllControllersAndDevices()
{
	for (const auto& [id, controller] : controllers) {
		delete controller;
	}

	controllers.clear();

	DeviceFactory::instance().DeleteAllDevices();

	FileSupport::UnreserveAll();
}

void ControllerManager::ResetAllControllers()
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

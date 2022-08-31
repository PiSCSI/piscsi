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

	return controller->AddLun(device);
}

AbstractController *ControllerManager::IdentifyController(int data) const
{
	for (const auto& controller : controllers) {
		if (data & (1 << controller.second->GetTargetId())) {
			return controller.second;
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
	for (const auto& controller : controllers) {
		delete controller.second;
	}

	controllers.clear();

	DeviceFactory::instance().DeleteAllDevices();

	FileSupport::UnreserveAll();
}

void ControllerManager::ResetAllControllers()
{
	for (const auto& controller : controllers) {
		controller.second->Reset();
	}
}

PrimaryDevice *ControllerManager::GetDeviceByIdAndLun(int id, int lun) const
{
	const AbstractController *controller = FindController(id);
	if (controller != nullptr) {
		return controller->GetDeviceForLun(lun);
	}

	return nullptr;
}

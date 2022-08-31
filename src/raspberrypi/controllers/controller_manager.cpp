//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "devices/primary_device.h"
#include "scsi_controller.h"
#include "controller_manager.h"

using namespace std;

unordered_map<int, AbstractController *> ControllerManager::controllers;

ControllerManager::~ControllerManager()
{
	DeleteAll();
}

ControllerManager& ControllerManager::instance()
{
	static ControllerManager instance;
	return instance;
}

void ControllerManager::CreateScsiController(BUS *bus, PrimaryDevice *device)
{
	AbstractController *controller = FindController(device->GetId());
	if (controller == nullptr) {
		controller = new ScsiController(bus, device->GetId());
		controllers[device->GetId()] = controller;
	}

	controller->SetDeviceForLun(device->GetLun(), device);
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

void ControllerManager::DeleteAll()
{
	for (const auto& controller : controllers) {
		delete controller.second;
	}

	controllers.clear();
}

void ControllerManager::ResetAll()
{
	for (const auto& controller : controllers) {
		controller.second->Reset();
	}
}

void ControllerManager::ClearAllLuns()
{
	for (auto controller : controllers) {
		controller.second->ClearLuns();
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

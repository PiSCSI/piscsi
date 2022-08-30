//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "abstract_controller.h"
#include "devices/primary_device.h"

unordered_map<int, AbstractController *> AbstractController::controllers;

AbstractController::AbstractController(BUS *bus, int target_id) : bus(bus), target_id(target_id) {
	controllers[target_id] = this;
}

PrimaryDevice *AbstractController::GetDeviceForLun(int lun) const {
	const auto& it = ctrl.luns.find(lun);
	return it == ctrl.luns.end() ? nullptr : it->second;
}

void AbstractController::SetDeviceForLun(int lun, PrimaryDevice *device)
{
	assert(lun < LUN_MAX);

	delete GetDeviceForLun(lun);

	if (device != nullptr) {
		ctrl.luns[lun] = device;
		device->SetController(this);
	}
	else {
		ctrl.luns.erase(lun);
	}
}

bool AbstractController::HasDeviceForLun(int lun) const
{
	return ctrl.luns.find(lun) != ctrl.luns.end();
}

// TODO Try not to expose the controller list by moving a part of the Process() code into the AbstractController class
// Or use the visitor pattern
const vector<AbstractController *> AbstractController::FindAll()
{
	vector<AbstractController *> result;

	for (auto controller : controllers) {
		result.push_back(controller.second);
	}

	return result;
}

AbstractController *AbstractController::FindController(int scsi_id)
{
	const auto& it = controllers.find(scsi_id);
	return it == controllers.end() ? nullptr : it->second;
}

void AbstractController::ClearLuns()
{
	for (auto controller : controllers) {
		controller.second->ctrl.luns.clear();
	}
}


void AbstractController::DeleteAll()
{
	ClearLuns();

	for (auto controller : controllers) {
		delete controller.second;
	}
}

void AbstractController::ResetAll()
{
	for (const auto& controller : controllers) {
		controller.second->Reset();
	}
}

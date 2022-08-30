//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "scsi_controller.h"
#include "controller_manager.h"
#include <vector>

unordered_map<int, AbstractController *> ControllerManager::controllers;

ControllerManager& ControllerManager::instance()
{
	static ControllerManager instance;
	return instance;
}

ScsiController *ControllerManager::CreateScsiController(BUS *bus, int target_id)
{
	return new ScsiController(bus, target_id);
}

// TODO Try not to expose the controller list by moving a part of the Process() code into the AbstractController class
// Or use the visitor pattern
const vector<AbstractController *> ControllerManager::FindAll()
{
	vector<AbstractController *> result;

	for (auto controller : controllers) {
		result.push_back(controller.second);
	}

	return result;
}

AbstractController *ControllerManager::FindController(int scsi_id)
{
	const auto& it = controllers.find(scsi_id);
	return it == controllers.end() ? nullptr : it->second;
}

void ControllerManager::DeleteAll()
{
	for (auto controller : controllers) {
		controller.second->ClearLuns();
		delete controller.second;
	}
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

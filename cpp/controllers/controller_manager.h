//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
// Keeps track of and manages the controllers
//
//---------------------------------------------------------------------------

#pragma once

#include "hal/bus.h"
#include "controllers/abstract_controller.h"
#include <unordered_map>
#include <unordered_set>
#include <memory>

using namespace std;

class PrimaryDevice;

class ControllerManager
{
public:

	ControllerManager() = default;
	~ControllerManager() = default;

	bool AttachToController(BUS&, int, shared_ptr<PrimaryDevice>);
	bool DeleteController(shared_ptr<AbstractController>);
	AbstractController::piscsi_shutdown_mode ProcessOnController(int) const;
	shared_ptr<AbstractController> FindController(int) const;
	bool HasController(int) const;
	unordered_set<shared_ptr<PrimaryDevice>> GetAllDevices() const;
	void DeleteAllControllers();
	shared_ptr<PrimaryDevice> GetDeviceByIdAndLun(int, int) const;
	void FlushCaches() const;

private:

	// Controllers mapped to their ID
	unordered_map<int, shared_ptr<AbstractController>> controllers;
};

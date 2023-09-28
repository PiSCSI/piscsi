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

class ScsiController;
class PrimaryDevice;

class ControllerManager
{
public:

	ControllerManager() = default;
	~ControllerManager() = default;

	shared_ptr<ScsiController> CreateScsiController(BUS&, int) const;
	bool AttachToController(BUS&, int, shared_ptr<PrimaryDevice>);
	bool DeleteController(const AbstractController&);
	void DeleteAllControllers();
	AbstractController::piscsi_shutdown_mode ProcessOnController(int) const;
	shared_ptr<AbstractController> FindController(int) const;
	bool HasController(int) const;
	unordered_set<shared_ptr<PrimaryDevice>> GetAllDevices() const;
	bool HasDeviceForIdAndLun(int, int) const;
	shared_ptr<PrimaryDevice> GetDeviceForIdAndLun(int, int) const;

	static int GetScsiIdMax() { return 8; }
	static int GetScsiLunMax() { return 32; }

private:

	// Controllers mapped to their device IDs
	unordered_map<int, shared_ptr<AbstractController>> controllers;
};

//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
// Keeps track of and manages the controllers
//
//---------------------------------------------------------------------------

#pragma once

#include <unordered_map>
#include <memory>

using namespace std;

class BUS;
class AbstractController;
class PrimaryDevice;

class ControllerManager
{

public:

	ControllerManager() = default;
	~ControllerManager() = default;
	ControllerManager(ControllerManager&) = delete;
	ControllerManager& operator=(const ControllerManager&) = delete;

	// Maximum number of controller devices
	static const int DEVICE_MAX = 8;

	bool CreateScsiController(shared_ptr<BUS>, PrimaryDevice *);
	shared_ptr<AbstractController> IdentifyController(int) const;
	shared_ptr<AbstractController> FindController(int) const;
	void DeleteAllControllers();
	void ResetAllControllers() const;
	PrimaryDevice *GetDeviceByIdAndLun(int, int) const;

	unordered_map<int, shared_ptr<AbstractController>> controllers;
};

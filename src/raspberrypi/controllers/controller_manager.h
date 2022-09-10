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
	ControllerManager() = default;
	~ControllerManager() = default;
	ControllerManager(ControllerManager&) = delete;
	ControllerManager& operator=(const ControllerManager&) = delete;

public:
	// Maximum number of controller devices
	static const int DEVICE_MAX = 8;

	static ControllerManager& instance();

	bool CreateScsiController(shared_ptr<BUS>, PrimaryDevice *) const;
	shared_ptr<AbstractController> IdentifyController(int) const;
	shared_ptr<AbstractController> FindController(int) const;
	void DeleteAllControllers() const;
	void ResetAllControllers() const;
	PrimaryDevice *GetDeviceByIdAndLun(int, int) const;

	inline static unordered_map<int, shared_ptr<AbstractController>> controllers;
};

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

class BUS;
class AbstractController;
class PrimaryDevice;

class ControllerManager
{
	ControllerManager() = default;
	~ControllerManager();
	ControllerManager(ControllerManager&) = delete;
	ControllerManager& operator=(const ControllerManager&) = delete;

public:
	// Maximum number of controller devices
	static const int DEVICE_MAX = 8;

	static ControllerManager& instance();

	bool CreateScsiController(BUS *, PrimaryDevice *);
	AbstractController *IdentifyController(int) const;
	AbstractController *FindController(int) const;
	void DeleteAllControllersAndDevices();
	void ResetAllControllers();
	PrimaryDevice *GetDeviceByIdAndLun(int, int) const;

	static std::unordered_map<int, AbstractController *> controllers;
};

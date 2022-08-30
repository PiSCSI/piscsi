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

#include <vector>
#include <unordered_map>

class BUS;
class AbstractController;
class PrimaryDevice;

class ControllerManager
{
	ControllerManager() {}
	~ControllerManager();

public:

	static ControllerManager& instance();

	void CreateScsiController(BUS *, PrimaryDevice *);
	AbstractController *IdentifyController(int);
	AbstractController *FindController(int);
	void DeleteAll();
	void ResetAll();
	void ClearAllLuns();
	PrimaryDevice *GetDeviceByIdAndLun(int, int) const;

	static std::unordered_map<int, AbstractController *> controllers;
};

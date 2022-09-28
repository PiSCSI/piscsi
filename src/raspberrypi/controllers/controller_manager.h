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

using namespace std; //NOSONAR Not relevant for rascsi

class BUS;
class AbstractController;
class PrimaryDevice;

class ControllerManager
{
	BUS& bus;

	unordered_map<int, shared_ptr<AbstractController>> controllers;

public:

	ControllerManager(BUS& bus) : bus(bus) {}
	~ControllerManager() = default;

	// Maximum number of controller devices
	static const int DEVICE_MAX = 8;

	bool CreateScsiController(PrimaryDevice *);
	bool DeleteController(int);
	shared_ptr<AbstractController> IdentifyController(int) const;
	shared_ptr<AbstractController> FindController(int) const;
	void DeleteAllControllers();
	void ResetAllControllers() const;
	PrimaryDevice *GetDeviceByIdAndLun(int, int) const;
};

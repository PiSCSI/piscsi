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
#include <unordered_set>
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

	explicit ControllerManager(BUS& bus) : bus(bus) {}
	~ControllerManager() = default;

	// Maximum number of controller devices
	static const int DEVICE_MAX = 8;

	bool AttachToScsiController(int, unique_ptr<PrimaryDevice>&);
	void DeleteController(shared_ptr<AbstractController>);
	shared_ptr<AbstractController> IdentifyController(int) const;
	shared_ptr<AbstractController> FindController(int) const;
	unordered_set<PrimaryDevice *> GetAllDevices() const;
	void DeleteAllControllers();
	void ResetAllControllers() const;
	shared_ptr<PrimaryDevice> GetDeviceByIdAndLun(int, int) const;
};

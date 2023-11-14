//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2021-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "hal/bus.h"
#include "controllers/controller_manager.h"
#include "devices/device_factory.h"
#include <unordered_set>

class DeviceFactory;
class PrimaryDevice;
class StorageDevice;
class CommandContext;

class PiscsiExecutor
{
public:

	PiscsiExecutor(BUS& bus, ControllerManager& controller_manager) : bus(bus), controller_manager(controller_manager) {}
	~PiscsiExecutor() = default;

	// TODO At least some of these methods should be private, currently they are directly called by the unit tests

	auto GetReservedIds() const { return reserved_ids; }

	bool ProcessDeviceCmd(const CommandContext&, const PbDeviceDefinition&, bool);
	bool ProcessCmd(const CommandContext&);
	bool Start(PrimaryDevice&, bool) const;
	bool Stop(PrimaryDevice&, bool) const;
	bool Eject(PrimaryDevice&, bool) const;
	bool Protect(PrimaryDevice&, bool) const;
	bool Unprotect(PrimaryDevice&, bool) const;
	bool Attach(const CommandContext&, const PbDeviceDefinition&, bool);
	bool Insert(const CommandContext&, const PbDeviceDefinition&, const shared_ptr<PrimaryDevice>&, bool) const;
	bool Detach(const CommandContext&, PrimaryDevice&, bool);
	void DetachAll();
	string SetReservedIds(string_view);
	bool ValidateImageFile(const CommandContext&, StorageDevice&, const string&) const;
	string PrintCommand(const PbCommand&, const PbDeviceDefinition&) const;
	string EnsureLun0(const PbCommand&) const;
	bool VerifyExistingIdAndLun(const CommandContext&, int, int) const;
	shared_ptr<PrimaryDevice> CreateDevice(const CommandContext&, const PbDeviceType, int, const string&) const;
	bool SetSectorSize(const CommandContext&, shared_ptr<PrimaryDevice>, int) const;

	static bool ValidateOperationAgainstDevice(const CommandContext&, const PrimaryDevice&, PbOperation);
	static bool ValidateIdAndLun(const CommandContext&, int, int);
	static bool SetProductData(const CommandContext&, const PbDeviceDefinition&, PrimaryDevice&);

private:

	static bool CheckForReservedFile(const CommandContext&, const string&);

	BUS& bus;

	ControllerManager& controller_manager;

	[[no_unique_address]] const DeviceFactory device_factory;

	unordered_set<int> reserved_ids;
};

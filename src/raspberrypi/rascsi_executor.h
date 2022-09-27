//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

class RascsiService;
class DeviceFactory;
class ControllerManager;
class PrimaryDevice;
class CommandContext;

class RascsiExecutor
{
	RascsiService& service;

	DeviceFactory& device_factory;

	ControllerManager& controller_manager;

public:

	RascsiExecutor(RascsiService& service, DeviceFactory& device_factory, ControllerManager& controller_manager)
		: service(service), device_factory(device_factory), controller_manager(controller_manager) {}
	~RascsiExecutor() = default;

	bool SetLogLevel(const string&);

	bool Detach(const CommandContext&, PrimaryDevice&, bool);
	void DetachAll();
};

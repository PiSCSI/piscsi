//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

class RascsiExecutor
{
	DeviceFactory& device_factory;

	ControllerManager& controller_manager;

public:

	RascsiExecutor(DeviceFactory& device_factory, ControllerManager& controller_manager)
		: device_factory(device_factory), controller_manager(controller_manager) {}
	~RascsiExecutor() = default;

	bool SetLogLevel(const string&);

	void DetachAll();
};

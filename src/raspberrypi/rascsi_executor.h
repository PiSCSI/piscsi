//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

class BUS;
class RascsiService;
class RascsiImage;
class DeviceFactory;
class ControllerManager;
class PrimaryDevice;
class CommandContext;
class SocketConnector;

class RascsiExecutor
{
	BUS& bus;

	RascsiService& service;

	RascsiImage& rascsi_image;

	DeviceFactory& device_factory;

	ControllerManager& controller_manager;

	SocketConnector socket_connector;

	const unordered_set<int>& reserved_ids;

public:

	RascsiExecutor(BUS& bus, RascsiService& service, RascsiImage& rascsi_image, DeviceFactory& device_factory,
			ControllerManager& controller_manager, const unordered_set<int>& reserved_ids)
		: bus(bus), service(service), rascsi_image(rascsi_image), device_factory(device_factory),
		  controller_manager(controller_manager), reserved_ids(reserved_ids) {}
	~RascsiExecutor() = default;

	bool ProcessCmd(const CommandContext&, const PbDeviceDefinition&, const PbCommand&, bool);
	bool SetLogLevel(const string&);
	bool Attach(const CommandContext&, const PbDeviceDefinition&, bool);
	bool Insert(const CommandContext&, const PbDeviceDefinition&, Device *, bool);
	bool Detach(const CommandContext&, PrimaryDevice&, bool);
	void DetachAll();
	bool ShutDown(const CommandContext&, const string&);
};

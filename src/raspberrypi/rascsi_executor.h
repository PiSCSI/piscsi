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
	RascsiService& service;

	RascsiImage& rascsi_image;

	DeviceFactory& device_factory;

	ControllerManager& controller_manager;

	SocketConnector socket_connector;

public:

	RascsiExecutor(RascsiService& service, RascsiImage& rascsi_image, DeviceFactory& device_factory,
			ControllerManager& controller_manager)
		: service(service), rascsi_image(rascsi_image), device_factory(device_factory),
		  controller_manager(controller_manager) {}
	~RascsiExecutor() = default;

	bool ProcessCmd(BUS&, const CommandContext&, const PbDeviceDefinition&, const PbCommand&,
			const std::unordered_set<int>&, bool);
	bool SetLogLevel(const string&);
	bool Attach(BUS&, const CommandContext&, const PbDeviceDefinition&, bool);
	bool Insert(const CommandContext&, const PbDeviceDefinition&, Device *, bool);
	bool Detach(const CommandContext&, PrimaryDevice&, bool);
	void DetachAll();
	bool ShutDown(const CommandContext&, const string&);
};

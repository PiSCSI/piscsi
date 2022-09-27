//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

class BUS;
class RascsiService;
class RascsiResponse;
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

	RascsiResponse& rascsi_response;

	RascsiImage& rascsi_image;

	DeviceFactory& device_factory;

	ControllerManager& controller_manager;

	SocketConnector socket_connector;

	unordered_set<int> reserved_ids;

public:

	RascsiExecutor(BUS& bus, RascsiService& service, RascsiResponse& rascsi_response, RascsiImage& rascsi_image,
			DeviceFactory& device_factory, ControllerManager& controller_manager)
		: bus(bus), service(service), rascsi_response(rascsi_response), rascsi_image(rascsi_image),
		  device_factory(device_factory), controller_manager(controller_manager) {}
	~RascsiExecutor() = default;

	const unordered_set<int> GetReservedIds() const { return reserved_ids; }

	bool ProcessCmd(const CommandContext&, const PbDeviceDefinition&, const PbCommand&, bool);
	bool ProcessCmd(const CommandContext&, const PbCommand&);
	bool SetLogLevel(const string&);
	bool Attach(const CommandContext&, const PbDeviceDefinition&, bool);
	bool Insert(const CommandContext&, const PbDeviceDefinition&, Device *, bool);
	bool Detach(const CommandContext&, PrimaryDevice&, bool);
	void DetachAll();
	bool ShutDown(const CommandContext&, const string&);
	string SetReservedIds(string_view);

	string ValidateLunSetup(const PbCommand&);
};

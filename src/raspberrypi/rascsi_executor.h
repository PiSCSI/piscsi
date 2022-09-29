//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "protobuf_serializer.h"

class RascsiResponse;
class RascsiImage;
class DeviceFactory;
class ControllerManager;
class PrimaryDevice;
class CommandContext;

class RascsiExecutor
{
	RascsiResponse& rascsi_response;

	RascsiImage& rascsi_image;

	DeviceFactory& device_factory;

	ControllerManager& controller_manager;

	ProtobufSerializer serializer;

	unordered_set<int> reserved_ids;

public:

	RascsiExecutor(RascsiResponse& rascsi_response, RascsiImage& rascsi_image, DeviceFactory& device_factory,
			ControllerManager& controller_manager)
		: rascsi_response(rascsi_response), rascsi_image(rascsi_image), device_factory(device_factory),
		  controller_manager(controller_manager) {}
	~RascsiExecutor() = default;

	unordered_set<int> GetReservedIds() const { return reserved_ids; }

	bool ProcessCmd(const CommandContext&, const PbDeviceDefinition&, const PbCommand&, bool);
	bool ProcessCmd(const CommandContext&, const PbCommand&);
	bool SetLogLevel(const string&) const;
	bool Attach(const CommandContext&, const PbDeviceDefinition&, bool);
	bool Insert(const CommandContext&, const PbDeviceDefinition&, Device&, bool) const;
	bool Detach(const CommandContext&, PrimaryDevice&, bool) const;
	void DetachAll();
	bool ShutDown(const CommandContext&, const string&);
	string SetReservedIds(string_view);
	bool OpenImageFile(const CommandContext&, Device&, const string&) const;

	string ValidateLunSetup(const PbCommand&) const;
};

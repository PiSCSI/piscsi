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

	bool ProcessCmd(CommandContext&, const PbDeviceDefinition&, const PbCommand&, bool);
	bool ProcessCmd(CommandContext&, const PbCommand&);
	bool SetLogLevel(const string&) const;
	bool Start(shared_ptr<PrimaryDevice>, bool) const;
	bool Stop(shared_ptr<PrimaryDevice>, bool) const;
	bool Eject(shared_ptr<PrimaryDevice>, bool) const;
	bool Protect(shared_ptr<PrimaryDevice>, bool) const;
	bool Unprotect(shared_ptr<PrimaryDevice>, bool) const;
	bool Attach(CommandContext&, const PbDeviceDefinition&, bool);
	bool Insert(CommandContext&, const PbDeviceDefinition&, shared_ptr<PrimaryDevice>, bool) const;
	bool Detach(CommandContext&, shared_ptr<PrimaryDevice>, bool) const;
	void DetachAll();
	bool ShutDown(CommandContext&, const string&);
	string SetReservedIds(string_view);
	bool ValidateImageFile(CommandContext&, shared_ptr<PrimaryDevice>, const string&, string&) const;
	void PrintCommand(const PbCommand&, const PbDeviceDefinition&, bool) const;
	string ValidateLunSetup(const PbCommand&) const;
	bool VerifyExistingIdAndLun(CommandContext&, int, int) const;
	shared_ptr<PrimaryDevice> CreateDevice(CommandContext&, const PbDeviceType, int, const string&) const;
	bool SetSectorSize(CommandContext&, const string& type, shared_ptr<PrimaryDevice>, int) const;

	static bool ValidationOperationAgainstDevice(CommandContext&, const shared_ptr<PrimaryDevice>,
			const PbOperation&);
	static bool ValidateIdAndLun(CommandContext&, int, int);
	static bool SetProductData(CommandContext&, const PbDeviceDefinition&, shared_ptr<PrimaryDevice>);

};

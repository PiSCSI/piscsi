//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "rascsi/rascsi_response.h"
#include "protobuf_serializer.h"

class RascsiImage;
class DeviceFactory;
class ControllerManager;
class PrimaryDevice;
class CommandContext;

class RascsiExecutor
{
public:

	RascsiExecutor(RascsiImage& rascsi_image, ControllerManager& controller_manager)
		: rascsi_image(rascsi_image), controller_manager(controller_manager) {}
	~RascsiExecutor() = default;

	unordered_set<int> GetReservedIds() const { return reserved_ids; }

	bool ProcessDeviceCmd(const CommandContext&, const PbDeviceDefinition&, const PbCommand&, bool);
	bool ProcessCmd(const CommandContext&, const PbCommand&);
	bool SetLogLevel(const string&) const;
	bool Start(PrimaryDevice&, bool) const;
	bool Stop(PrimaryDevice&, bool) const;
	bool Eject(PrimaryDevice&, bool) const;
	bool Protect(PrimaryDevice&, bool) const;
	bool Unprotect(PrimaryDevice&, bool) const;
	bool Attach(const CommandContext&, const PbDeviceDefinition&, bool);
	bool Insert(const CommandContext&, const PbDeviceDefinition&, const shared_ptr<PrimaryDevice>&, bool) const;
	bool Detach(const CommandContext&, const shared_ptr<PrimaryDevice>&, bool) const;
	void DetachAll();
	bool ShutDown(const CommandContext&, const string&);
	string SetReservedIds(string_view);
	bool ValidateImageFile(const CommandContext&, StorageDevice&, const string&, string&) const;
	void PrintCommand(const PbCommand&, const PbDeviceDefinition&, bool) const;
	string ValidateLunSetup(const PbCommand&) const;
	bool VerifyExistingIdAndLun(const CommandContext&, int, int) const;
	shared_ptr<PrimaryDevice> CreateDevice(const CommandContext&, const PbDeviceType, int, const string&) const;
	bool SetSectorSize(const CommandContext&, shared_ptr<PrimaryDevice>, int) const;

	static bool ValidateOperationAgainstDevice(const CommandContext&, const PrimaryDevice&, const PbOperation&);
	static bool ValidateIdAndLun(const CommandContext&, int, int);
	static bool SetProductData(const CommandContext&, const PbDeviceDefinition&, PrimaryDevice&);

private:

	RascsiResponse rascsi_response;

	RascsiImage& rascsi_image;

	ControllerManager& controller_manager;

	DeviceFactory device_factory;

	ProtobufSerializer serializer;

	unordered_set<int> reserved_ids;
};

//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "devices/device_logger.h"
#include "piscsi/command_context.h"
#include "piscsi/piscsi_service.h"
#include "piscsi/piscsi_image.h"
#include "piscsi/piscsi_response.h"
#include "generated/piscsi_interface.pb.h"
#include <vector>
#include <string>
#include <filesystem>
#include <atomic>

using namespace std;
using namespace filesystem;

class BUS;
class ControllerManager;
class PiscsiExecutor;

class Piscsi
{
	using optargs_type = vector<pair<int, string>>;

	static const int DEFAULT_PORT = 6868;

public:

	Piscsi() = default;
	~Piscsi() = default;

	int run(const vector<char *>&);

private:

	void Banner(const vector<char *>&) const;
	bool InitBus() const;
	static void Cleanup();
	void ReadAccessToken(const path&) const;
	void LogDevices(string_view) const;
	PbDeviceType ParseDeviceType(const string&) const;
	static void TerminationHandler(int);
	optargs_type ParseArguments(const vector<char *>&, int&) const;
	void CreateInitialDevices(const optargs_type&) const;
	void WaitForNotBusy() const;

	// TODO Should not be static and should be moved to PiscsiService
	static bool ExecuteCommand(const CommandContext&, const PbCommand&);

	DeviceLogger device_logger;

	// A static instance is needed because of the signal handler
	static inline shared_ptr<BUS> bus;

	// TODO These fields should not be static

	static inline PiscsiService service;

	static inline PiscsiImage piscsi_image;

	const static inline PiscsiResponse piscsi_response;

	static inline shared_ptr<ControllerManager> controller_manager;

	static inline shared_ptr<PiscsiExecutor> executor;

	// Processing flag
	static inline atomic<bool> active;

	// Some versions of spdlog do not support get_log_level(), so we have to remember the level
	static inline string current_log_level = "info";

	static inline string access_token;
};

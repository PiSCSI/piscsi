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
#include "spdlog/sinks/stdout_color_sinks.h"
#include <span>
#include <vector>
#include <string>
#include <atomic>

using namespace std;

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

	int run(span<char *>);

private:

	void Banner(span<char *>) const;
	bool InitBus() const;
	static void Cleanup();
	void ReadAccessToken(const path&);
	void LogDevices(string_view) const;
	static void TerminationHandler(int);
	optargs_type ParseArguments(span<char *>, int&);
	void CreateInitialDevices(const optargs_type&) const;
	void Process();
	void WaitForNotBusy() const;

	bool ExecuteCommand(const CommandContext&, const PbCommand&);

	bool SetLogLevel(const string&) const;

	const shared_ptr<spdlog::logger> logger = spdlog::stdout_color_mt("piscsi stdout logger");

	DeviceLogger device_logger;

	// Processing flag
	atomic<bool> active;

	string current_log_level = "info";

	string access_token;

	PiscsiResponse piscsi_response;

	// A static instance is needed because of the signal handler
	static inline shared_ptr<BUS> bus;

	// TODO These fields should not be static

	static inline PiscsiService service;

	static inline PiscsiImage piscsi_image;

	static inline shared_ptr<ControllerManager> controller_manager;

	static inline shared_ptr<PiscsiExecutor> executor;
};

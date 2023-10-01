//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "controllers/controller_manager.h"
#include "controllers/abstract_controller.h"
#include "piscsi/command_context.h"
#include "piscsi/piscsi_service.h"
#include "piscsi/piscsi_image.h"
#include "piscsi/piscsi_response.h"
#include "piscsi/piscsi_executor.h"
#include "generated/piscsi_interface.pb.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <span>
#include <string>
#include <atomic>

using namespace std;

class BUS;

class Piscsi
{
	static const int DEFAULT_PORT = 6868;

public:

	Piscsi() = default;
	~Piscsi() = default;

	int run(span<char *>);

private:

	void Banner(span<char *>) const;
	bool InitBus();
	void CleanUp();
	void ReadAccessToken(const path&);
	void LogDevices(string_view) const;
	static void TerminationHandler(int);
	string ParseArguments(span<char *>, PbCommand&, int&, string&);
	void Process();
	bool IsNotBusy() const;

	void ShutDown(AbstractController::piscsi_shutdown_mode);

	bool ExecuteCommand(const CommandContext&);

	bool SetLogLevel(const string&) const;

	const shared_ptr<spdlog::logger> logger = spdlog::stdout_color_mt("piscsi stdout logger");

	static PbDeviceType ParseDeviceType(const string&);

	// Processing flag
	atomic_bool target_is_active;

	string access_token;

	PiscsiImage piscsi_image;

	PiscsiResponse response;

	PiscsiService service;

	unique_ptr<PiscsiExecutor> executor;

	ControllerManager controller_manager;

	unique_ptr<BUS> bus;

	// Required for the termination handler
	static inline Piscsi *instance;
};

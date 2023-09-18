//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "piscsi/command_context.h"
#include "piscsi/piscsi_service.h"
#include "piscsi/piscsi_image.h"
#include "piscsi/piscsi_response.h"
#include "piscsi/piscsi_executor.h"
#include "generated/piscsi_interface.pb.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <span>
#include <vector>
#include <string>
#include <atomic>

using namespace std;

class BUS;
class ControllerManager;

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
	bool InitBus();
	void Cleanup();
	void ReadAccessToken(const path&);
	void LogDevices(string_view) const;
	static void TerminationHandler(int);
	optargs_type ParseArguments(span<char *>, int&);
	void CreateInitialDevices(const optargs_type&);
	void Process();
	bool IsNotBusy() const;

	bool ExecuteCommand(const CommandContext&);

	bool SetLogLevel(const string&) const;

	const shared_ptr<spdlog::logger> logger = spdlog::stdout_color_mt("piscsi stdout logger");

	// Processing flag
	atomic_bool target_is_active;

	string access_token;

	PiscsiImage piscsi_image;

	PiscsiResponse piscsi_response;

	PiscsiService service;

	unique_ptr<PiscsiExecutor> executor;

	shared_ptr<ControllerManager> controller_manager;

	shared_ptr<BUS> bus;

	// Required for the termination handler
	static inline Piscsi *instance;
};

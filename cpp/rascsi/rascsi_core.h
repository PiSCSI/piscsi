//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "rascsi/command_context.h"
#include "rascsi/rascsi_service.h"
#include "rascsi/rascsi_image.h"
#include "rascsi/rascsi_response.h"
#include "generated/rascsi_interface.pb.h"
#include <vector>
#include <string>

using namespace std;

class BUS;
class ControllerManager;
class RascsiExecutor;

class Rascsi
{
	using optarg_queue_type = vector<pair<int, string>>;

	static const int DEFAULT_PORT = 6868;

public:

	Rascsi() = default;
	~Rascsi() = default;

	int run(const vector<char *>&) const;

private:

	void Banner(const vector<char *>&) const;
	bool InitBus() const;
	static void Cleanup();
	bool ReadAccessToken(const char *) const;
	void LogDevices(string_view) const;
	static void TerminationHandler(int);
	bool ParseArguments(const vector<char *>&, int&, optarg_queue_type&) const;
	bool CreateInitialDevices(const optarg_queue_type&) const;

	// TODO Should not be static and should be moved to RascsiService
	static bool ExecuteCommand(const CommandContext&, const PbCommand&);

	// A static instance is needed because of the signal handler
	static inline shared_ptr<BUS> bus;

	// TODO These fields should not be static

	static inline RascsiService service;

	static inline RascsiImage rascsi_image;

	const static inline RascsiResponse rascsi_response;

	static inline shared_ptr<ControllerManager> controller_manager;

	static inline shared_ptr<RascsiExecutor> executor;

	// Processing flag
	static inline volatile bool active;

	// Some versions of spdlog do not support get_log_level(), so we have to remember the level
	static inline string current_log_level = "info";

	static inline string access_token;
};

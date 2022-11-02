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
#include "rascsi_interface.pb.h"
#include <deque>
#include <vector>
#include <string>

using namespace std;

class Rascsi
{
	using optarg_value_type = pair<int, string>;
	using optarg_queue_type = deque<optarg_value_type>;

	static const int DEFAULT_PORT = 6868;
	static const char COMPONENT_SEPARATOR = ':';

public:

	Rascsi() = default;
	~Rascsi() = default;

	int run(const vector<char *>&);

private:

	void Banner(const vector<char *>&) const;
	bool InitBus() const;
	void Reset() const;
	bool ReadAccessToken(const char *) const;
	void LogDevices(string_view) const;
	bool ProcessId(const string&, int&, int&) const;
	bool ParseArguments(const vector<char *>&, int&, optarg_queue_type&);
	bool CreateInitialDevices(const optarg_queue_type&) const;

	static bool ExecuteCommand(const CommandContext&, const PbCommand&);

	// TODO Get rid of static fields

	// Some versions of spdlog do not support get_log_level(), so we have to remember the level
	static inline string current_log_level = "info";

	static inline string access_token;
};

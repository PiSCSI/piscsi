//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "rascsi_interface.pb.h"
#include <functional>
#include <thread>

class CommandContext;

using namespace std;

class RascsiService
{
	using callback = function<bool(const CommandContext&, rascsi_interface::PbCommand&)>;

	callback execute;

	int service_socket = -1;

	thread monthread;

	static volatile bool running;

public:

	RascsiService() = default;
	~RascsiService() = default;

	bool Init(const callback&, int);
	void Cleanup() const;

	bool IsRunning() const { return running; }
	void SetRunning(bool b) const { running = b; }

	void Execute() const;

	PbCommand ReadCommand(CommandContext&) const;

	static void KillHandler(int) { running = false; }
};

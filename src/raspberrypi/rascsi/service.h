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
class ProtobufSerializer;

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
	~RascsiService(); //NOSONAR Destructor is needed to release external resource

	bool Init(callback, int);

	bool IsRunning() const { return running; }
	void SetRunning(bool b) const { running = b; }

	void Execute();

	int ReadCommand(const ProtobufSerializer&, rascsi_interface::PbCommand&) const;

	static void KillHandler(int) { running = false; }
};

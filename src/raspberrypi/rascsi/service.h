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
#include <thread>

class CommandContext;
class ProtobufSerializer;

class RascsiService
{
	bool (*execute)(rascsi_interface::PbCommand&, CommandContext&) = nullptr;

	int service_socket = -1;

	thread monthread;

	static volatile bool running;

public:

	RascsiService() = default;
	~RascsiService();

	bool Init(bool (ExecuteCommand)(rascsi_interface::PbCommand&, CommandContext&), int);

	bool IsRunning() const { return running; }
	void SetRunning(bool b) const { running = b; }

	void Execute();

	int ReadCommand(ProtobufSerializer&, rascsi_interface::PbCommand&);

	static void KillHandler(int) { running = false; }
};

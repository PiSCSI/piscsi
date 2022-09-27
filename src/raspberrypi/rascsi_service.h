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
#include <mutex>

class CommandContext;
class ProtobufSerializer;

class RascsiService
{
	bool (*execute)(rascsi_interface::PbCommand&, CommandContext&) = nullptr;

	int monsocket = -1;

	thread monthread;
	mutex ctrl_mutex;

	static bool is_instantiated;

	static volatile bool is_running;

public:

	RascsiService();
	~RascsiService();

	bool Init(bool (ExecuteCommand)(rascsi_interface::PbCommand&, CommandContext&), int);

	void Lock() { ctrl_mutex.lock(); }
	void Unlock() { ctrl_mutex.unlock(); }

	bool IsRunning() const { return is_running; }
	void SetRunning(bool b) const { is_running = b; }

	void Execute();

	int ReadCommand(ProtobufSerializer&, rascsi_interface::PbCommand&);

	static void KillHandler(int) { is_running = false; }
};

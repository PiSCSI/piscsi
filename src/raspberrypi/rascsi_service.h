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
	static bool is_instantiated;

	static volatile bool is_running;

	static int monsocket;

	thread monthread;
	mutex ctrl_mutex;

public:

	RascsiService();
	~RascsiService();

	bool Init(bool (ExecuteCommand)(rascsi_interface::PbCommand&, CommandContext&), int);

	void Lock() { ctrl_mutex.lock(); }
	void Unlock() { ctrl_mutex.unlock(); }

	static bool IsRunning() { return is_running; }
	static void SetRunning(bool b) { is_running = b; }

	static void CommandThread(bool (*)(rascsi_interface::PbCommand&, CommandContext&));

	static int ReadCommand(ProtobufSerializer&, rascsi_interface::PbCommand&);

	static void KillHandler(int) { is_running = false; }
};

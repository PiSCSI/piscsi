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
#include "protobuf_serializer.h"
#include <pthread.h>

class CommandContext;

class RascsiService
{
	static bool is_instantiated;

	static volatile bool is_running;

	static int monsocket;

	static pthread_t monthread;
	pthread_mutex_t ctrl_mutex;

public:

	RascsiService();
	~RascsiService();

	bool Init(bool (ExecuteCommand)(rascsi_interface::PbCommand&, CommandContext&), int);

	void Lock() { pthread_mutex_lock(&ctrl_mutex); }
	void Unlock() { pthread_mutex_unlock(&ctrl_mutex); }

	static bool IsRunning() { return is_running; }
	static void SetRunning(bool b) { is_running = b; }

	static void *MonThread(bool (*)(rascsi_interface::PbCommand&, CommandContext&));

	static int ReadCommand(ProtobufSerializer&, rascsi_interface::PbCommand&);

	static void KillHandler(int) { is_running = false; }
};

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
#include "localizer.h"
#include "socket_connector.h"
#include <pthread.h>

class CommandContext;

class RascsiService
{
	static bool is_instantiated;

	static volatile bool is_running;

	static const Localizer localizer;

	static const SocketConnector connector;

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

	static int ReadCommand(rascsi_interface::PbCommand&, int);

	static void KillHandler(int) {
		is_running = false;
	}
};

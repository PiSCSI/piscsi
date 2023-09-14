//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "shared/piscsi_util.h"
#include "shared/piscsi_exceptions.h"
#include "command_context.h"
#include "localizer.h"
#include "piscsi_service.h"
#include <spdlog/spdlog.h>
#include <netinet/in.h>
#include <csignal>

using namespace piscsi_interface;
using namespace piscsi_util;

void PiscsiService::Cleanup() const
{
	running = false;

	if (service_socket != -1) {
		close(service_socket);
	}
}

bool PiscsiService::Init(const callback& cb, int port)
{
	if (port <= 0 || port > 65535) {
		return false;
	}

	// Create socket for monitor
	sockaddr_in server = {};
	service_socket = socket(PF_INET, SOCK_STREAM, 0);
	if (service_socket == -1) {
		spdlog::error("Unable to create socket");
		return false;
	}

	server.sin_family = PF_INET;
	server.sin_port = htons((uint16_t)port);
	server.sin_addr.s_addr = htonl(INADDR_ANY);

	// Allow address reuse
	if (int yes = 1; setsockopt(service_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
		return false;
	}

	signal(SIGPIPE, SIG_IGN);

	if (bind(service_socket, (sockaddr *)&server, sizeof(sockaddr_in)) < 0) {
		cerr << "Error: Port " << port << " is in use, is piscsi or rascsi already running?" << endl;
		return false;
	}

	execute = cb;

	monthread = thread(&PiscsiService::Execute, this);
	monthread.detach();

	// Interrupt handler settings
	return signal(SIGINT, KillHandler) != SIG_ERR && signal(SIGHUP, KillHandler) != SIG_ERR
			&& signal(SIGTERM, KillHandler) != SIG_ERR;
}

void PiscsiService::Execute() const
{
#ifdef __linux__
    // Scheduler Settings
	sched_param schedparam;
	schedparam.sched_priority = 0;
	sched_setscheduler(0, SCHED_IDLE, &schedparam);
#endif

	// Set the affinity to a specific processor core
	FixCpu(2);

	// Wait for the execution to start
	const timespec ts = { .tv_sec = 0, .tv_nsec = 1000};
	while (!running) {
		nanosleep(&ts, nullptr);
	}

	// Set up the monitor socket to receive commands
	listen(service_socket, 1);

	sockaddr client = {};
	socklen_t socklen = sizeof(client);

	while (true) {
		const int fd = accept(service_socket, &client, &socklen);
		if (fd == -1) {
			spdlog::warn("accept() failed");
			continue;
		}

		CommandContext context(fd);
		try {
			if (context.ReadCommand()) {
				execute(context);
			}
		}
		catch(const io_exception& e) {
			spdlog::warn(e.what());
		}

		close(fd);
	}
}

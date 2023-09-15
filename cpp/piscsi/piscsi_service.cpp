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
#include <sys/socket.h>
#include <netinet/in.h>
#include <csignal>

using namespace piscsi_interface;
using namespace piscsi_util;

void PiscsiService::Cleanup() const
{
	running = false;

	if (service_socket != -1) {
		shutdown(service_socket, SHUT_RDWR);
		close(service_socket);
	}
}

bool PiscsiService::Init(const callback& cb, int port)
{
	if (port <= 0 || port > 65535) {
		return false;
	}

	// Create socket for monitor
	service_socket = socket(PF_INET, SOCK_STREAM, 0);
	if (service_socket == -1) {
		spdlog::error("Unable to create socket");
		return false;
	}

	// Allow address reuse
	if (const int yes = 1; setsockopt(service_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
		return false;
	}

	const sockaddr_in server = { .sin_family = PF_INET, .sin_port = htons((uint16_t)port),
			.sin_addr { .s_addr = INADDR_ANY }, .sin_zero = {} };
	if (bind(service_socket, (const sockaddr *)&server, sizeof(sockaddr_in)) < 0) {
		cerr << "Error: Port " << port << " is in use, is piscsi already running?" << endl;
		return false;
	}

	signal(SIGPIPE, SIG_IGN);

	execute = cb;

	monthread = jthread([this] () { Execute(); } );

	return true;
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

	while (running) {
		const int fd = accept(service_socket, nullptr, nullptr);
		if (fd == -1) {
			continue;
		}

		CommandContext context(fd);
		try {
			context.ReadCommand();
			if (context.GetCommand().operation() != PbOperation::NO_OPERATION) {
				execute(context);
			}
		}
		catch(const io_exception& e) {
			spdlog::warn(e.what());
		}

		close(fd);
	}
}

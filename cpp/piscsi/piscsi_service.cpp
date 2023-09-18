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
#include "piscsi_service.h"
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <csignal>
#include <future>

using namespace piscsi_util;

void PiscsiService::Stop()
{
	if (service_socket != -1) {
		shutdown(service_socket, SHUT_RDWR);
		close(service_socket);

		service_socket = -1;
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

	if (const sockaddr_in server = { .sin_family = PF_INET, .sin_port = htons((uint16_t)port),
			.sin_addr { .s_addr = INADDR_ANY }, .sin_zero = {} };
		bind(service_socket, reinterpret_cast<const sockaddr *>(&server), sizeof(sockaddr_in)) < 0) { //NOSONAR bind() API requires reinterpret_cast
		cerr << "Error: Port " << port << " is in use, is piscsi already running?" << endl;
		return false;
	}

	signal(SIGPIPE, SIG_IGN);

	execute = cb;

	return true;
}

void PiscsiService::Start() const
{
	// Set up the monitor socket to receive commands
	listen(service_socket, 1);

	const auto& socket_listener = async(launch::async, [this] () { Execute(); } );
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

	while (service_socket != -1) {
		const int fd = accept(service_socket, nullptr, nullptr);
		if (fd != -1) {
			ExecuteCommand(fd);
			close(fd);
		}
	}
}

void PiscsiService::ExecuteCommand(int fd) const
{
	CommandContext context(fd);
	try {
		context.ReadCommand();
		execute(context);
	}
	catch(const io_exception& e) {
		spdlog::warn(e.what());
	}
}

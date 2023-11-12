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
#include <arpa/inet.h>
#include <csignal>
#include <cassert>

using namespace piscsi_util;

string PiscsiService::Init(const callback& cb, int port)
{
	assert(service_socket == -1);

	if (port <= 0 || port > 65535) {
		return "Invalid port number " + to_string(port);
	}

	service_socket = socket(PF_INET, SOCK_STREAM, 0);
	if (service_socket == -1) {
		return "Unable to create service socket: " + string(strerror(errno));
	}

	if (const int yes = 1; setsockopt(service_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
		Stop();
		return "Can't reuse address";
	}

	sockaddr_in server = {};
	server.sin_family = PF_INET;
	server.sin_port = htons((uint16_t)port);
	server.sin_addr.s_addr = INADDR_ANY;
    if (bind(service_socket, reinterpret_cast<const sockaddr*>(&server), //NOSONAR bit_cast is not supported by the bullseye compiler
        static_cast<socklen_t>(sizeof(sockaddr_in))) < 0) {
		Stop();
		return "Port " + to_string(port) + " is in use, is piscsi already running?";
	}

	if (listen(service_socket, 2) == -1) {
		Stop();
		return "Can't listen to service socket: " + string(strerror(errno));
	}

	execute = cb;

	return "";
}

void PiscsiService::Start()
{
	assert(service_socket != -1);

	service_thread = jthread([this] () { Execute(); } );
}

void PiscsiService::Stop()
{
	assert(service_socket != -1);

	shutdown(service_socket, SHUT_RD);
	close(service_socket);

	service_socket = -1;
}

void PiscsiService::Execute() const
{
#ifdef __linux__
	// Run this thread with very low priority
	sched_param schedparam = { .sched_priority = 0 };
	sched_setscheduler(0, SCHED_IDLE, &schedparam);
#endif

	// TODO Accept more than one command instead of closing the socket after a single command
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
		if (context.ReadCommand()) {
			execute(context);
		}
	}
	catch(const io_exception& e) {
		spdlog::warn(e.what());

		// Try to return an error message (this may fail if the exception was caused when returning the actual result)
		PbResult result;
		result.set_msg(e.what());
		try {
			context.WriteResult(result);
		}
		catch(const io_exception&) { //NOSONAR Not handled on purpose
			// Ignore
		}
	}
}

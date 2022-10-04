//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "log.h"
#include "rascsi_exceptions.h"
#include "command_context.h"
#include "protobuf_serializer.h"
#include "localizer.h"
#include "rasutil.h"
#include "rascsi_service.h"
#include <netinet/in.h>
#include <csignal>

using namespace rascsi_interface;

volatile bool RascsiService::running = false;

void RascsiService::Cleanup() const
{
	if (service_socket != -1) {
		close(service_socket);
	}
}

bool RascsiService::Init(const callback& cb, int port)
{
	// Create socket for monitor
	sockaddr_in server = {};
	service_socket = socket(PF_INET, SOCK_STREAM, 0);
	if (service_socket == -1) {
		LOGERROR("Unable to create socket")
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
		cerr << "Error: Port " << port << " is in use, is rascsi already running?" << endl;
		return false;
	}

	execute = cb;

	monthread = thread(&RascsiService::Execute, this);
	monthread.detach();

	// Interrupt handler settings
	return signal(SIGINT, KillHandler) != SIG_ERR && signal(SIGHUP, KillHandler) != SIG_ERR
			&& signal(SIGTERM, KillHandler) != SIG_ERR;
}

void RascsiService::Execute() const
{
#ifdef __linux__
    // Scheduler Settings
	sched_param schedparam;
	schedparam.sched_priority = 0;
	sched_setscheduler(0, SCHED_IDLE, &schedparam);
#endif

	// Set the affinity to a specific processor core
	ras_util::FixCpu(2);

	// Wait for the execution to start
	while (!running) {
		usleep(1);
	}

	// Set up the monitor socket to receive commands
	listen(service_socket, 1);

	while (true) {
		CommandContext context;

		try {
			PbCommand command = ReadCommand(context);
			if (context.IsValid()) {
				execute(context, command);
			}
		}
		catch(const io_exception& e) {
			LOGWARN("%s", e.get_msg().c_str())

            // Fall through
		}

		context.Cleanup();
	}
}

PbCommand RascsiService::ReadCommand(CommandContext& context) const
{
	// Wait for connection
	sockaddr client = {};
	socklen_t socklen = sizeof(client);
	int fd = accept(service_socket, &client, &socklen);
	if (fd == -1) {
		throw io_exception("accept() failed");
	}

	PbCommand command;

	// Read magic string
	vector<byte> magic(6);
	if (size_t bytes_read = context.GetSerializer().ReadBytes(fd, magic);
		bytes_read != magic.size() || memcmp(magic.data(), "RASCSI", magic.size())) {
		throw io_exception("Invalid magic");
	}

	// Fetch the command
	context.GetSerializer().DeserializeMessage(fd, command);

	context.SetFd(fd);

	return command;
}


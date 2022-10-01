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

RascsiService::~RascsiService()
{
	if (service_socket != -1) {
		close(service_socket);
	}
}

bool RascsiService::Init(bool (e)(PbCommand&, CommandContext&), int port)
{
	// Create socket for monitor
	sockaddr_in server = {};
	service_socket = socket(PF_INET, SOCK_STREAM, 0);
	if (service_socket == -1) {
		LOGERROR("Unable to create socket");
		return false;
	}

	server.sin_family = PF_INET;
	server.sin_port = htons(port);
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

	execute = e;

	monthread = thread(&RascsiService::Execute, this);
	monthread.detach();

	// Interrupt handler settings
	return signal(SIGINT, KillHandler) != SIG_ERR && signal(SIGHUP, KillHandler) != SIG_ERR
			&& signal(SIGTERM, KillHandler) != SIG_ERR;
}

void RascsiService::Execute()
{
#ifdef __linux
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

	ProtobufSerializer serializer;
	Localizer localizer;
	while (true) {
		CommandContext context(serializer, localizer, -1, "");

		try {
			PbCommand command;
			context.fd = ReadCommand(serializer, command);
			if (context.fd == -1) {
				continue;
			}

			execute(command, context);
		}
		catch(const io_exception& e) {
			LOGWARN("%s", e.get_msg().c_str())

			// Fall through
		}

		if (context.fd != -1) {
			close(context.fd);
		}
	}
}

int RascsiService::ReadCommand(ProtobufSerializer& serializer, PbCommand& command)
{
	// Wait for connection
	sockaddr client = {};
	socklen_t socklen = sizeof(client);
	int fd = accept(service_socket, &client, &socklen);
	if (fd < 0) {
		throw io_exception("accept() failed");
	}

	// Read magic string
	vector<byte> magic(6);
	size_t bytes_read = serializer.ReadBytes(fd, magic);
	if (!bytes_read) {
		return -1;
	}
	if (bytes_read != magic.size() || memcmp(magic.data(), "RASCSI", magic.size())) {
		throw io_exception("Invalid magic");
	}

	// Fetch the command
	serializer.DeserializeMessage(fd, command);

	return fd;
}

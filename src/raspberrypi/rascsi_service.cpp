//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "config.h"
#include "log.h"
#include "rascsi_exceptions.h"
#include "command_context.h"
#include "rascsi_service.h"
#include <netinet/in.h>
#include <csignal>

#define FPRT(fp, ...) fprintf(fp, __VA_ARGS__ )

using namespace rascsi_interface;

const Localizer RascsiService::localizer;

const SocketConnector RascsiService::socket_connector;

volatile bool RascsiService::is_running = false;

int RascsiService::monsocket = -1;
pthread_t RascsiService::monthread;
pthread_mutex_t RascsiService::ctrl_mutex;

RascsiService::~RascsiService()
{
	pthread_mutex_destroy(&ctrl_mutex);

	if (monsocket != -1) {
		close(monsocket);
	}
}

bool RascsiService::Init(bool (execute)(PbCommand&, CommandContext&), int port)
{
	if (int result = pthread_mutex_init(&ctrl_mutex, nullptr); result != EXIT_SUCCESS){
		LOGERROR("Unable to create a mutex. Error code: %d", result)
		return false;
	}

	// Create socket for monitor
	sockaddr_in server = {};
	monsocket = socket(PF_INET, SOCK_STREAM, 0);
	server.sin_family = PF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = htonl(INADDR_ANY);

	// allow address reuse
	if (int yes = 1; setsockopt(monsocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
		return false;
	}

	signal(SIGPIPE, SIG_IGN);

	if (bind(monsocket, (sockaddr *)&server, sizeof(sockaddr_in)) < 0) {
		FPRT(stderr, "Error: Port %d is in use, is rascsi already running?\n", port);
		return false;
	}

	// Create Monitor Thread
	pthread_create(&monthread, nullptr, (void* (*)(void*))MonThread, (void *)execute);

	// Interrupt handler settings
	if (signal(SIGINT, KillHandler) == SIG_ERR || signal(SIGHUP, KillHandler) == SIG_ERR
			|| signal(SIGTERM, KillHandler) == SIG_ERR) {
			return false;
	}

	return true;
}

// Pin the thread to a specific CPU
void RascsiService::FixCpu(int cpu)
{
#ifdef __linux
	// Get the number of CPUs
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	sched_getaffinity(0, sizeof(cpu_set_t), &cpuset);
	int cpus = CPU_COUNT(&cpuset);

	// Set the thread affinity
	if (cpu < cpus) {
		CPU_ZERO(&cpuset);
		CPU_SET(cpu, &cpuset);
		sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
	}
#endif
}

void *RascsiService::MonThread(bool (execute)(PbCommand&, CommandContext&))
{
    // Scheduler Settings
	sched_param schedparam;
	schedparam.sched_priority = 0;
	sched_setscheduler(0, SCHED_IDLE, &schedparam);

	// Set the affinity to a specific processor core
	FixCpu(2);

	// Wait for the execution to start
	while (!is_running) {
		usleep(1);
	}

	// Set up the monitor socket to receive commands
	listen(monsocket, 1);

	while (true) {
		CommandContext context(socket_connector, localizer, -1, "");

		try {
			PbCommand command;
			context.fd = socket_connector.ReadCommand(command, monsocket);
			if (context.fd == -1) {
				continue;
			}

			if (!execute(command, context)) {
				continue;
			}
		}
		catch(const io_exception& e) {
			LOGWARN("%s", e.get_msg().c_str())

			// Fall through
		}

		if (context.fd >= 0) {
			close(context.fd);
		}
	}

	return nullptr;
}

//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//	Copyright (C) 2020-2022 Contributors to the RaSCSI project
//	[ RaSCSI main ]
//
//---------------------------------------------------------------------------

#include "config.h"
#include "log.h"
#include "controllers/controller_manager.h"
#include "controllers/scsi_controller.h"
#include "devices/device_factory.h"
#include "devices/disk.h"
#include "hal/gpiobus.h"
#include "hal/gpiobus_factory.h"
#include "hal/sbc_version.h"
#include "hal/systimer.h"
#include "rascsi_version.h"
#include "rascsi_exceptions.h"
#include "protobuf_serializer.h"
#include "protobuf_util.h"
#include "rascsi/rascsi_executor.h"
#include "rascsi/rascsi_response.h"
#include "rascsi/rascsi_image.h"
#include "rascsi/rascsi_service.h"
#include "rascsi/rascsi_core.h"
#include "rasutil.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <netinet/in.h>
#include <csignal>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <list>

using namespace std;
using namespace spdlog;
using namespace rascsi_interface;
using namespace ras_util;
using namespace protobuf_util;

//---------------------------------------------------------------------------
//
// Variable declarations
// TODO Make these fields class fields
//
//---------------------------------------------------------------------------
RascsiService service;
shared_ptr<GPIOBUS> bus;
DeviceFactory device_factory;
shared_ptr<ControllerManager> controller_manager;
RascsiImage rascsi_image;
shared_ptr<RascsiResponse> rascsi_response;
shared_ptr<RascsiExecutor> executor;

void Rascsi::Banner(const vector<char *>& args) const
{
	cout << ras_util::Banner("Reloaded");
	cout << "Connect type: " << CONNECT_DESC << '\n' << flush;

	if ((args.size() > 1 && strcmp(args[1], "-h") == 0) || (args.size() > 1 && strcmp(args[1], "--help") == 0)){
		cout << "\nUsage: " << args[0] << " [-idn[:m] FILE] ...\n\n";
		cout << " n is SCSI device ID (0-7).\n";
		cout << " m is the optional logical unit (LUN) (0-31).\n";
		cout << " FILE is a disk image file, \"daynaport\", \"bridge\", \"printer\" or \"services\".\n\n";
		cout << " Image type is detected based on file extension if no explicit type is specified.\n";
		cout << "  hd1 : SCSI-1 HD image (Non-removable generic SCSI-1 HD image)\n";
		cout << "  hds : SCSI HD image (Non-removable generic SCSI HD image)\n";
		cout << "  hdr : SCSI HD image (Removable generic HD image)\n";
		cout << "  hda : SCSI HD image (Apple compatible image)\n";
		cout << "  hdn : SCSI HD image (NEC compatible image)\n";
		cout << "  hdi : SCSI HD image (Anex86 HD image)\n";
		cout << "  nhd : SCSI HD image (T98Next HD image)\n";
		cout << "  mos : SCSI MO image (MO image)\n";
		cout << "  iso : SCSI CD image (ISO 9660 image)\n" << flush;

		exit(EXIT_SUCCESS);
	}
}

bool Rascsi::InitBus() const
{
#ifdef USE_SEL_EVENT_ENABLE
	SBC_Version::Init();
#endif

	// GPIOBUS creation
	bus = GPIOBUS_Factory::Create();

	// GPIO Initialization
	if (!bus->Init()) {
		return false;
	}

	bus->Reset();

	controller_manager = make_shared<ControllerManager>(bus);
	rascsi_response = make_shared<RascsiResponse>(device_factory, *controller_manager, ScsiController::LUN_MAX);
	executor = make_shared<RascsiExecutor>(*rascsi_response, rascsi_image, device_factory, *controller_manager);

	return true;
}

void Cleanup()
{
	executor->DetachAll();

	service.Cleanup();

	bus->Cleanup();
}

void Rascsi::Reset() const
{
	controller_manager->ResetAllControllers();

	bus->Reset();
}

bool Rascsi::ReadAccessToken(const char *filename) const
{
	struct stat st;
	if (stat(filename, &st) || !S_ISREG(st.st_mode)) {
		cerr << "Can't access token file '" << optarg << "'" << endl;
		return false;
	}

	if (st.st_uid || st.st_gid || (st.st_mode & (S_IROTH | S_IWOTH | S_IRGRP | S_IWGRP))) {
		cerr << "Access token file '" << optarg << "' must be owned by root and readable by root only" << endl;
		return false;
	}

	ifstream token_file(filename);
	if (token_file.fail()) {
		cerr << "Can't open access token file '" << optarg << "'" << endl;
		return false;
	}

	getline(token_file, access_token);
	if (token_file.fail()) {
		cerr << "Can't read access token file '" << optarg << "'" << endl;
		return false;
	}

	if (access_token.empty()) {
		cerr << "Access token file '" << optarg << "' must not be empty" << endl;
		return false;
	}

	return true;
}

void Rascsi::LogDevices(string_view devices) const
{
	stringstream ss(devices.data());
	string line;

	while (getline(ss, line, '\n')) {
		LOGINFO("%s", line.c_str())
	}
}

void TerminationHandler(int signum)
{
	Cleanup();

	exit(signum);
}

bool Rascsi::ProcessId(const string& id_spec, int& id, int& unit) const
{
	if (const size_t separator_pos = id_spec.find(COMPONENT_SEPARATOR); separator_pos == string::npos) {
		if (!GetAsInt(id_spec, id) || id < 0 || id >= 8) {
			cerr << optarg << ": Invalid device ID (0-7)" << endl;
			return false;
		}

		unit = 0;
	}
	else if (!GetAsInt(id_spec.substr(0, separator_pos), id) || id < 0 || id > 7 ||
			!GetAsInt(id_spec.substr(separator_pos + 1), unit) || unit < 0 || unit >= ScsiController::LUN_MAX) {
		cerr << optarg << ": Invalid unit (0-" << (ScsiController::LUN_MAX - 1) << ")" << endl;
		return false;
	}

	return true;
}

bool Rascsi::ParseArguments(const vector<char *>& args, int& port, optarg_queue_type& post_process) const
{
	int block_size = 0;
	string name;

	opterr = 1;
	int opt;
	while ((opt = getopt(static_cast<int>(args.size()), args.data(), "-Iib:d:n:p:r:t:z:D:F:L:P:R:C:v")) != -1) {
		switch (opt) {
			// The following options can not be processed until AFTER
			// the 'bus' object is created and configured
			case 'i':
			case 'I':
			case 'd':
			case 'D':
			case 'R':
			case 'n':
			case 'r':
			case 't':
			case 'F':
			case 'z':
			{
				string optarg_str = (optarg == nullptr) ? "" : string(optarg);
				post_process.emplace_back(opt ,optarg_str);
				continue;
			}
			case 'b': {
				if (!GetAsInt(optarg, block_size)) {
					cerr << "Invalid block size " << optarg << endl;
					return false;
				}
				continue;
			}

			case 'L':
				current_log_level = optarg;
				continue;

			case 'p':
				if (!GetAsInt(optarg, port) || port <= 0 || port > 65535) {
					cerr << "Invalid port " << optarg << ", port must be between 1 and 65535" << endl;
					return false;
				}
				continue;

			case 'P':
				if (!ReadAccessToken(optarg)) {
					return false;
				}
				continue;

			case 'v':
				cout << rascsi_get_version_string() << endl;
				exit(0);

			case 1:
			{
				// Encountered filename
				const string optarg_str = (optarg == nullptr) ? "" : string(optarg);
				post_process.emplace_back(opt, optarg_str);
				continue;
			}

			default:
				return false;
		}

		if (optopt) {
			return false;
		}
	}

	return true;
}

bool Rascsi::CreateInitialDevices(const optarg_queue_type& optarg_queue) const
{
	PbCommand command;
	int id = -1;
	int unit = -1;
	PbDeviceType type = UNDEFINED;
	int block_size = 0;
	string name;
	string log_level;

	const char *locale = setlocale(LC_MESSAGES, "");
	if (locale == nullptr || !strcmp(locale, "C")) {
		locale = "en";
	}


	opterr = 1;
	for (const auto& [option, value] : optarg_queue) {
		switch (option) {
			// The two options below are kind of a compound option with two letters
			case 'i':
			case 'I':
				id = -1;
				unit = -1;
				continue;

			case 'd':
			case 'D': {
				if (!ProcessId(value, id, unit)) {
					return false;
				}
				continue;
			}

			case 'z':
				locale = value.c_str();
				continue;

			case 'F': {
				if (const string result = rascsi_image.SetDefaultFolder(value); !result.empty()) {
					cerr << result << endl;
					return false;
				}
				continue;
			}

			case 'R':
				int depth;
				if (!GetAsInt(value, depth) || depth < 0) {
					cerr << "Invalid image file scan depth " << value << endl;
					return false;
				}
				rascsi_image.SetDepth(depth);
				continue;

			case 'n':
				name = value;
				continue;

			case 'r': {
					string error = executor->SetReservedIds(value);
					if (!error.empty()) {
						cerr << error << endl;
						return false;
					}
				}
				continue;

			case 't': {
					string t = value;
					transform(t.begin(), t.end(), t.begin(), ::toupper);
					if (!PbDeviceType_Parse(t, &type)) {
						cerr << "Illegal device type '" << value << "'" << endl;
						return false;
					}
				}
				continue;

			case 1:
				// Encountered filename
				break;
		}

		// Set up the device data
		PbDeviceDefinition *device = command.add_devices();
		device->set_id(id);
		device->set_unit(unit);
		device->set_type(type);
		device->set_block_size(block_size);

		ParseParameters(*device, value);

		if (size_t separator_pos = name.find(COMPONENT_SEPARATOR); separator_pos != string::npos) {
			device->set_vendor(name.substr(0, separator_pos));
			name = name.substr(separator_pos + 1);
			separator_pos = name.find(COMPONENT_SEPARATOR);
			if (separator_pos != string::npos) {
				device->set_product(name.substr(0, separator_pos));
				device->set_revision(name.substr(separator_pos + 1));
			}
			else {
				device->set_product(name);
			}
		}
		else {
			device->set_vendor(name);
		}

		id = -1;
		type = UNDEFINED;
		block_size = 0;
		name = "";
	}

	// Attach all specified devices
	command.set_operation(ATTACH);

	if (CommandContext context(locale); !executor->ProcessCmd(context, command)) {
		return false;
	}

	// Display and log the device list
	PbServerInfo server_info;
	rascsi_response->GetDevices(server_info, rascsi_image.GetDefaultFolder());
	const list<PbDevice>& devices = { server_info.devices_info().devices().begin(), server_info.devices_info().devices().end() };
	const string device_list = ListDevices(devices);
	LogDevices(device_list);
	cout << device_list << flush;

	return true;
}

bool Rascsi::ExecuteCommand(const CommandContext& context, const PbCommand& command)
{
	if (!access_token.empty() && access_token != GetParam(command, "token")) {
		return context.ReturnLocalizedError(LocalizationKey::ERROR_AUTHENTICATION, UNAUTHORIZED);
	}

	if (!PbOperation_IsValid(command.operation())) {
		LOGERROR("Received unknown command with operation opcode %d", command.operation())

		return context.ReturnLocalizedError(LocalizationKey::ERROR_OPERATION, UNKNOWN_OPERATION);
	}

	LOGTRACE("Received %s command", PbOperation_Name(command.operation()).c_str())

	PbResult result;
	ProtobufSerializer serializer;

	switch(command.operation()) {
		case LOG_LEVEL: {
			const string log_level = GetParam(command, "level");
			if (const bool status = executor->SetLogLevel(log_level); !status) {
				context.ReturnLocalizedError(LocalizationKey::ERROR_LOG_LEVEL, log_level);
			}
			else {
				current_log_level = log_level;

				context.ReturnStatus();
			}
			break;
		}

		case DEFAULT_FOLDER: {
			if (const string status = rascsi_image.SetDefaultFolder(GetParam(command, "folder")); !status.empty()) {
				context.ReturnStatus(false, status);
			}
			else {
				context.ReturnStatus();
			}
			break;
		}

		case DEVICES_INFO: {
			rascsi_response->GetDevicesInfo(result, command, rascsi_image.GetDefaultFolder());
			serializer.SerializeMessage(context.GetFd(), result);
			break;
		}

		case DEVICE_TYPES_INFO: {
			result.set_allocated_device_types_info(rascsi_response->GetDeviceTypesInfo(result).release());
			serializer.SerializeMessage(context.GetFd(), result);
			break;
		}

		case SERVER_INFO: {
			result.set_allocated_server_info(rascsi_response->GetServerInfo(
					result, executor->GetReservedIds(), current_log_level, rascsi_image.GetDefaultFolder(),
					GetParam(command, "folder_pattern"), GetParam(command, "file_pattern"),
					rascsi_image.GetDepth()).release());
			serializer.SerializeMessage(context.GetFd(), result);
			break;
		}

		case VERSION_INFO: {
			result.set_allocated_version_info(rascsi_response->GetVersionInfo(result).release());
			serializer.SerializeMessage(context.GetFd(), result);
			break;
		}

		case LOG_LEVEL_INFO: {
			result.set_allocated_log_level_info(rascsi_response->GetLogLevelInfo(result, current_log_level).release());
			serializer.SerializeMessage(context.GetFd(), result);
			break;
		}

		case DEFAULT_IMAGE_FILES_INFO: {
			result.set_allocated_image_files_info(rascsi_response->GetAvailableImages(result,
					rascsi_image.GetDefaultFolder(), GetParam(command, "folder_pattern"),
					GetParam(command, "file_pattern"), rascsi_image.GetDepth()).release());
			serializer.SerializeMessage(context.GetFd(), result);
			break;
		}

		case IMAGE_FILE_INFO: {
			if (string filename = GetParam(command, "file"); filename.empty()) {
				context.ReturnLocalizedError( LocalizationKey::ERROR_MISSING_FILENAME);
			}
			else {
				auto image_file = make_unique<PbImageFile>();
				const bool status = rascsi_response->GetImageFile(*image_file.get(), rascsi_image.GetDefaultFolder(), filename);
				if (status) {
					result.set_status(true);
					result.set_allocated_image_file_info(image_file.get());
					serializer.SerializeMessage(context.GetFd(), result);
				}
				else {
					context.ReturnLocalizedError(LocalizationKey::ERROR_IMAGE_FILE_INFO);
				}
			}
			break;
		}

		case NETWORK_INTERFACES_INFO: {
			result.set_allocated_network_interfaces_info(rascsi_response->GetNetworkInterfacesInfo(result).release());
			serializer.SerializeMessage(context.GetFd(), result);
			break;
		}

		case MAPPING_INFO: {
			result.set_allocated_mapping_info(rascsi_response->GetMappingInfo(result).release());
			serializer.SerializeMessage(context.GetFd(), result);
			break;
		}

		case OPERATION_INFO: {
			result.set_allocated_operation_info(rascsi_response->GetOperationInfo(result,
					rascsi_image.GetDepth()).release());
			serializer.SerializeMessage(context.GetFd(), result);
			break;
		}

		case RESERVED_IDS_INFO: {
			result.set_allocated_reserved_ids_info(rascsi_response->GetReservedIds(result,
					executor->GetReservedIds()).release());
			serializer.SerializeMessage(context.GetFd(), result);
			break;
		}

		case SHUT_DOWN: {
			if (executor->ShutDown(context, GetParam(command, "mode"))) {
				TerminationHandler(0);
			}
			break;
		}

		default: {
			// Wait until we become idle
			const timespec ts = { .tv_sec = 0, .tv_nsec = 500'000'000};
			while (active) {
				nanosleep(&ts, nullptr);
			}

			executor->ProcessCmd(context, command);
			break;
		}
	}

	return true;
}

int Rascsi::run(const vector<char *>& args) const
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	// added setvbuf to override stdout buffering, so logs are written immediately and not when the process exits.
	setvbuf(stdout, nullptr, _IONBF, 0);

	Banner(args);

	int port = DEFAULT_PORT;
	optarg_queue_type optarg_queue;
	if (!ParseArguments(args, port, optarg_queue)) {
		return -1;
	}

	// Note that current_log_level may have been modified by ParseArguments()
	executor->SetLogLevel(current_log_level);

	// Create a thread-safe stdout logger to process the log messages
	const auto logger = stdout_color_mt("rascsi stdout logger");

	if (!InitBus()) {
		return EPERM;
	}

	if (!service.Init(&ExecuteCommand, port)) {
		return EPERM;
	}

	// We need to wait to create the devices until after the bus/controller/etc
	// objects have been created.
	if (!CreateInitialDevices(optarg_queue)) {
		Cleanup();
		return -1;
	}

	// Signal handler to detach all devices on a KILL or TERM signal
	struct sigaction termination_handler;
	termination_handler.sa_handler = TerminationHandler;
	sigemptyset(&termination_handler.sa_mask);
	termination_handler.sa_flags = 0;
	sigaction(SIGINT, &termination_handler, nullptr);
	sigaction(SIGTERM, &termination_handler, nullptr);

	// Reset
	Reset();

    // Set the affinity to a specific processor core
	FixCpu(3);

	sched_param schparam;
#ifdef USE_SEL_EVENT_ENABLE
	// Scheduling policy setting (highest priority)
	schparam.sched_priority = sched_get_priority_max(SCHED_FIFO);
	sched_setscheduler(0, SCHED_FIFO, &schparam);
#else
	cout << "Note: No RaSCSI hardware support, only client interface calls are supported" << endl;
#endif

	// Start execution
	service.SetRunning(true);

	// Main Loop
	while (service.IsRunning()) {
#ifdef USE_SEL_EVENT_ENABLE
		// SEL signal polling
		if (!bus->PollSelectEvent()) {
			// Stop on interrupt
			if (errno == EINTR) {
				break;
			}
			continue;
		}

		// Get the bus
		bus->Acquire();
#else
		bus->Acquire();
		if (!bus->GetSEL()) {
			const timespec ts = { .tv_sec = 0, .tv_nsec = 0};
			nanosleep(&ts, nullptr);
			continue;
		}
#endif

        // Wait until BSY is released as there is a possibility for the
        // initiator to assert it while setting the ID (for up to 3 seconds)
		if (bus->GetBSY()) {
			const uint32_t now = SysTimer::GetTimerLow();
			while ((SysTimer::GetTimerLow() - now) < 3'000'000) {
				bus->Acquire();
				if (!bus->GetBSY()) {
					break;
				}
			}
		}

		// Stop because the bus is busy or another device responded
		if (bus->GetBSY() || !bus->GetSEL()) {
			continue;
		}

		int initiator_id = -1;

		// The initiator and target ID
		const uint8_t id_data = bus->GetDAT();

		BUS::phase_t phase = BUS::phase_t::busfree;

		// Identify the responsible controller
		shared_ptr<AbstractController> controller = controller_manager->IdentifyController(id_data);
		if (controller != nullptr) {
			initiator_id = controller->ExtractInitiatorId(id_data);

			if (controller->Process(initiator_id) == BUS::phase_t::selection) {
				phase = BUS::phase_t::selection;
			}
		}

		// Return to bus monitoring if the selection phase has not started
		if (phase != BUS::phase_t::selection) {
			continue;
		}

		// Start target device
		active = true;

#if !defined(USE_SEL_EVENT_ENABLE) && defined(__linux__)
		// Scheduling policy setting (highest priority)
		schparam.sched_priority = sched_get_priority_max(SCHED_FIFO);
		sched_setscheduler(0, SCHED_FIFO, &schparam);
#endif

		// Loop until the bus is free
		while (service.IsRunning()) {
			// Target drive
			phase = controller->Process(initiator_id);

			// End when the bus is free
			if (phase == BUS::phase_t::busfree) {
				break;
			}
		}

#if !defined(USE_SEL_EVENT_ENABLE) && defined(__linux__)
		// Set the scheduling priority back to normal
		schparam.sched_priority = 0;
		sched_setscheduler(0, SCHED_OTHER, &schparam);
#endif

		// End the target travel
		active = false;
	}

	return 0;
}

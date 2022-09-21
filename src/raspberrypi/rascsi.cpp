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
#include "controllers/controller_manager.h"
#include "devices/device_factory.h"
#include "devices/device.h"
#include "devices/disk.h"
#include "devices/file_support.h"
#include "hal/gpiobus.h"
#include "hal/systimer.h"
#include "rascsi_exceptions.h"
#include "protobuf_util.h"
#include "rascsi_version.h"
#include "rascsi_response.h"
#include "rasutil.h"
#include "rascsi_image.h"
#include "rascsi_interface.pb.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <pthread.h>
#include <netinet/in.h>
#include <csignal>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <list>
#include <map>

using namespace std;
using namespace spdlog;
using namespace rascsi_interface;
using namespace ras_util;
using namespace protobuf_util;

//---------------------------------------------------------------------------
//
//  Constant declarations
//
//---------------------------------------------------------------------------
#define FPRT(fp, ...) fprintf(fp, __VA_ARGS__ )
static const int DEFAULT_PORT = 6868;
static const char COMPONENT_SEPARATOR = ':';

//---------------------------------------------------------------------------
//
//	Variable declarations
//
//---------------------------------------------------------------------------
static volatile bool running;		// Running flag
static volatile bool active;		// Processing flag
shared_ptr<GPIOBUS> bus;			// GPIO Bus
int monsocket;						// Monitor Socket
pthread_t monthread;				// Monitor Thread
pthread_mutex_t ctrl_mutex;					// Semaphore for the ctrl array
static void *MonThread(void *param);
string current_log_level;			// Some versions of spdlog do not support get_log_level()
string access_token;
unordered_set<int> reserved_ids;
DeviceFactory& device_factory = DeviceFactory::instance();
ControllerManager controller_manager;
RascsiImage rascsi_image;
RascsiResponse rascsi_response(&device_factory, &rascsi_image);
void DetachAll();

//---------------------------------------------------------------------------
//
//	Signal Processing
//
//---------------------------------------------------------------------------
void KillHandler(int)
{
	// Stop instruction
	running = false;
}

//---------------------------------------------------------------------------
//
//	Banner Output
//
//---------------------------------------------------------------------------
void Banner(int argc, char* argv[])
{
	FPRT(stdout,"SCSI Target Emulator RaSCSI Reloaded ");
	FPRT(stdout,"version %s (%s, %s)\n",
		rascsi_get_version_string().c_str(),
		__DATE__,
		__TIME__);
	FPRT(stdout,"Powered by XM6 TypeG Technology / ");
	FPRT(stdout,"Copyright (C) 2016-2020 GIMONS\n");
	FPRT(stdout,"Copyright (C) 2020-2022 Contributors to the RaSCSI Reloaded project\n");
	FPRT(stdout,"Connect type: %s\n", CONNECT_DESC);

	if ((argc > 1 && strcmp(argv[1], "-h") == 0) ||
		(argc > 1 && strcmp(argv[1], "--help") == 0)){
		FPRT(stdout,"\n");
		FPRT(stdout,"Usage: %s [-idn[:m] FILE] ...\n\n", argv[0]);
		FPRT(stdout," n is SCSI device ID (0-7).\n");
		FPRT(stdout," m is the optional logical unit (LUN) (0-31).\n");
		FPRT(stdout," FILE is a disk image file, \"daynaport\", \"bridge\", \"printer\" or \"services\".\n\n");
		FPRT(stdout," Image type is detected based on file extension if no explicit type is specified.\n");
		FPRT(stdout,"  hd1 : SCSI-1 HD image (Non-removable generic SCSI-1 HD image)\n");
		FPRT(stdout,"  hds : SCSI HD image (Non-removable generic SCSI HD image)\n");
		FPRT(stdout,"  hdr : SCSI HD image (Removable generic HD image)\n");
		FPRT(stdout,"  hdn : SCSI HD image (NEC GENUINE)\n");
		FPRT(stdout,"  hdi : SCSI HD image (Anex86 HD image)\n");
		FPRT(stdout,"  nhd : SCSI HD image (T98Next HD image)\n");
		FPRT(stdout,"  mos : SCSI MO image (MO image)\n");
		FPRT(stdout,"  iso : SCSI CD image (ISO 9660 image)\n");

		exit(EXIT_SUCCESS);
	}
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------

bool InitService(int port)
{
	if (int result = pthread_mutex_init(&ctrl_mutex, nullptr); result != EXIT_SUCCESS){
		LOGERROR("Unable to create a mutex. Error code: %d", result)
		return false;
	}

	// Create socket for monitor
	sockaddr_in server;
	monsocket = socket(PF_INET, SOCK_STREAM, 0);
	memset(&server, 0, sizeof(server));
	server.sin_family = PF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = htonl(INADDR_ANY);

	// allow address reuse
	if (int yes = 1; setsockopt(monsocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
		return false;
	}

	signal(SIGPIPE, SIG_IGN);

	// Bind
	if (bind(monsocket, (sockaddr *)&server, sizeof(sockaddr_in)) < 0) {
		FPRT(stderr, "Error: Port %d is in use, is rascsi already running?\n", port);
		return false;
	}

	// Create Monitor Thread
	pthread_create(&monthread, nullptr, MonThread, nullptr);

	// Interrupt handler settings
	if (signal(SIGINT, KillHandler) == SIG_ERR) {
		return false;
	}
	if (signal(SIGHUP, KillHandler) == SIG_ERR) {
		return false;
	}
	if (signal(SIGTERM, KillHandler) == SIG_ERR) {
		return false;
	}

	running = false;
	active = false;

	return true;
}

bool InitBus()
{
	// GPIOBUS creation
	bus = make_shared<GPIOBUS>();

	// GPIO Initialization
	if (!bus->Init()) {
		return false;
	}

	// Bus Reset
	bus->Reset();

	return true;
}

void Cleanup()
{
	DetachAll();

	// Clean up and discard the bus
	if (bus != nullptr) {
		bus->Cleanup();
	}

	// Close the monitor socket
	if (monsocket >= 0) {
		close(monsocket);
	}

	pthread_mutex_destroy(&ctrl_mutex);
}

void Reset()
{
	controller_manager.ResetAllControllers();

	bus->Reset();
}

bool ReadAccessToken(const char *filename)
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

	ifstream token_file(filename, ifstream::in);
	if (token_file.fail()) {
		cerr << "Can't open access token file '" << optarg << "'" << endl;
		return false;
	}

	getline(token_file, access_token);
	if (token_file.fail()) {
		token_file.close();
		cerr << "Can't read access token file '" << optarg << "'" << endl;
		return false;
	}

	if (access_token.empty()) {
		token_file.close();
		cerr << "Access token file '" << optarg << "' must not be empty" << endl;
		return false;
	}

	token_file.close();

	return true;
}

string ValidateLunSetup(const PbCommand& command)
{
	// Mapping of available LUNs (bit vector) to devices
	map<uint32_t, uint32_t> luns;

	// Collect LUN bit vectors of new devices
	for (const auto& device : command.devices()) {
		luns[device.id()] |= 1 << device.unit();
	}

	// Collect LUN bit vectors of existing devices
	for (const auto device : device_factory.GetAllDevices()) {
		luns[device->GetId()] |= 1 << device->GetLun();
	}

	// LUN 0 must exist for all devices
	for (auto const& [id, lun]: luns) {
		if (!(lun & 0x01)) {
			return "LUN 0 is missing for device ID " + to_string(id);
		}
	}

	return "";
}

bool SetLogLevel(const string& log_level)
{
	if (log_level == "trace") {
		set_level(level::trace);
	}
	else if (log_level == "debug") {
		set_level(level::debug);
	}
	else if (log_level == "info") {
		set_level(level::info);
	}
	else if (log_level == "warn") {
		set_level(level::warn);
	}
	else if (log_level == "err") {
		set_level(level::err);
	}
	else if (log_level == "critical") {
		set_level(level::critical);
	}
	else if (log_level == "off") {
		set_level(level::off);
	}
	else {
		return false;
	}

	current_log_level = log_level;

	LOGINFO("Set log level to '%s'", current_log_level.c_str())

	return true;
}

void LogDevices(string_view devices)
{
	stringstream ss(devices.data());
	string line;

	while (getline(ss, line, '\n')) {
		LOGINFO("%s", line.c_str())
	}
}

string SetReservedIds(string_view ids)
{
	list<string> ids_to_reserve;
	stringstream ss(ids.data());
    string id;
    while (getline(ss, id, ',')) {
    	if (!id.empty()) {
    		ids_to_reserve.push_back(id);
    	}
    }

    unordered_set<int> reserved;
    for (string id_to_reserve : ids_to_reserve) {
    	int id;
 		if (!GetAsInt(id_to_reserve, id) || id > 7) {
 			return "Invalid ID " + id_to_reserve;
 		}

 		if (controller_manager.FindController(id) != nullptr) {
 			return "ID " + id_to_reserve + " is currently in use";
 		}

 		reserved.insert(id);
    }

    reserved_ids = reserved;

    if (!reserved_ids.empty()) {
    	string s;
    	bool isFirst = true;
    	for (auto const& reserved_id : reserved_ids) {
    		if (!isFirst) {
    			s += ", ";
    		}
    		isFirst = false;
    		s += to_string(reserved_id);
    	}

    	LOGINFO("Reserved ID(s) set to %s", s.c_str())
    }
    else {
    	LOGINFO("Cleared reserved ID(s)")
    }

	return "";
}

void DetachAll()
{
	controller_manager.DeleteAllControllers();
	device_factory.DeleteAllDevices();
	FileSupport::UnreserveAll();

	LOGINFO("Detached all devices")
}

bool Attach(const CommandContext& context, const PbDeviceDefinition& pb_device, bool dryRun)
{
	const int id = pb_device.id();
	const int unit = pb_device.unit();
	const PbDeviceType type = pb_device.type();

	if (controller_manager.GetDeviceByIdAndLun(id, unit) != nullptr) {
		return ReturnLocalizedError(context, ERROR_DUPLICATE_ID, to_string(id), to_string(unit));
	}

	if (unit >= AbstractController::LUN_MAX) {
		return ReturnLocalizedError(context, ERROR_INVALID_LUN, to_string(unit), to_string(AbstractController::LUN_MAX));
	}

	string filename = GetParam(pb_device, "file");

	PrimaryDevice *device = device_factory.CreateDevice(type, filename, id);
	if (device == nullptr) {
		if (type == UNDEFINED) {
			return ReturnLocalizedError(context, ERROR_MISSING_DEVICE_TYPE, filename);
		}
		else {
			return ReturnLocalizedError(context, ERROR_UNKNOWN_DEVICE_TYPE, PbDeviceType_Name(type));
		}
	}

	// If no filename was provided the medium is considered removed
	auto file_support = dynamic_cast<FileSupport *>(device);
	if (file_support != nullptr) {
		device->SetRemoved(filename.empty());
	}
	else {
		device->SetRemoved(false);
	}

	device->SetLun(unit);

	try {
		if (!pb_device.vendor().empty()) {
			device->SetVendor(pb_device.vendor());
		}
		if (!pb_device.product().empty()) {
			device->SetProduct(pb_device.product());
		}
		if (!pb_device.revision().empty()) {
			device->SetRevision(pb_device.revision());
		}
	}
	catch(const illegal_argument_exception& e) { //NOSONAR This exception is handled properly
		return ReturnStatus(context, false, e.get_msg());
	}

	if (pb_device.block_size()) {
		auto disk = dynamic_cast<Disk *>(device);
		if (disk != nullptr && disk->IsSectorSizeConfigurable()) {
			if (!disk->SetConfiguredSectorSize(pb_device.block_size())) {
				device_factory.DeleteDevice(device);

				return ReturnLocalizedError(context, ERROR_BLOCK_SIZE, to_string(pb_device.block_size()));
			}
		}
		else {
			device_factory.DeleteDevice(device);

			return ReturnLocalizedError(context, ERROR_BLOCK_SIZE_NOT_CONFIGURABLE, PbDeviceType_Name(type));
		}
	}

	// File check (type is HD, for removable media drives, CD and MO the medium (=file) may be inserted later
	if (file_support != nullptr && !device->IsRemovable() && filename.empty()) {
		device_factory.DeleteDevice(device);

		return ReturnLocalizedError(context, ERROR_MISSING_FILENAME, PbDeviceType_Name(type));
	}

	Filepath filepath;
	if (file_support != nullptr && !filename.empty()) {
		filepath.SetPath(filename.c_str());
		string initial_filename = filepath.GetPath();

		int id;
		int unit;
		if (FileSupport::GetIdsForReservedFile(filepath, id, unit)) {
			device_factory.DeleteDevice(device);

			return ReturnLocalizedError(context, ERROR_IMAGE_IN_USE, filename, to_string(id), to_string(unit));
		}

		try {
			try {
				file_support->Open(filepath);
			}
			catch(const file_not_found_exception&) {
				// If the file does not exist search for it in the default image folder
				filepath.SetPath(string(rascsi_image.GetDefaultImageFolder() + "/" + filename).c_str());

				if (FileSupport::GetIdsForReservedFile(filepath, id, unit)) {
					device_factory.DeleteDevice(device);

					return ReturnLocalizedError(context, ERROR_IMAGE_IN_USE, filename, to_string(id), to_string(unit));
				}

				file_support->Open(filepath);
			}
		}
		catch(const io_exception& e) {
			device_factory.DeleteDevice(device);

			return ReturnLocalizedError(context, ERROR_FILE_OPEN, initial_filename, e.get_msg());
		}

		file_support->ReserveFile(filepath, device->GetId(), device->GetLun());
	}

	// Only non read-only devices support protect/unprotect
	// This operation must not be executed before Open() because Open() overrides some settings.
	if (device->IsProtectable() && !device->IsReadOnly()) {
		device->SetProtected(pb_device.protected_());
	}

	// Stop the dry run here, before permanently modifying something
	if (dryRun) {
		device_factory.DeleteDevice(device);

		return true;
	}

	unordered_map<string, string> params = { pb_device.params().begin(), pb_device.params().end() };
	if (!device->SupportsFile()) {
		params.erase("file");
	}
	if (!device->Init(params)) {
		device_factory.DeleteDevice(device);

		return ReturnLocalizedError(context, ERROR_INITIALIZATION, PbDeviceType_Name(type), to_string(id), to_string(unit));
	}

	pthread_mutex_lock(&ctrl_mutex);

	if (!controller_manager.CreateScsiController(bus, device)) {
		pthread_mutex_unlock(&ctrl_mutex);

		return ReturnLocalizedError(context, ERROR_SCSI_CONTROLLER);
	}
	pthread_mutex_unlock(&ctrl_mutex);

	string msg = "Attached ";
	if (device->IsReadOnly()) {
		msg += "read-only ";
	}
	else if (device->IsProtectable() && device->IsProtected()) {
		msg += "protected ";
	}
	msg += device->GetType() + " device, ID " + to_string(id) + ", unit " + to_string(unit);
	LOGINFO("%s", msg.c_str())

	return true;
}

bool Detach(const CommandContext& context, PrimaryDevice *device, bool dryRun)
{
	if (!device->GetLun()) {
		for (const Device *d : device_factory.GetAllDevices()) {
			// LUN 0 can only be detached if there is no other LUN anymore
			if (d->GetId() == device->GetId() && d->GetLun()) {
				return ReturnLocalizedError(context, ERROR_LUN0);
			}
		}
	}

	if (!dryRun) {
		// Remember data that going to be deleted but are used for logging
		int id = device->GetId();
		int lun = device->GetLun();
		string type = device->GetType();

		if (auto file_support = dynamic_cast<FileSupport *>(device); file_support != nullptr) {
			file_support->UnreserveFile();
		}

		// Delete the existing unit
		pthread_mutex_lock(&ctrl_mutex);
		if (!controller_manager.FindController(id)->DeleteDevice(device)) {
			pthread_mutex_unlock(&ctrl_mutex);

			return ReturnLocalizedError(context, ERROR_DETACH);
		}
		device_factory.DeleteDevice(device);
		pthread_mutex_unlock(&ctrl_mutex);

		LOGINFO("Detached %s device with ID %d, unit %d", type.c_str(), id, lun)
	}

	return true;
}

bool Insert(const CommandContext& context, const PbDeviceDefinition& pb_device, Device *device, bool dryRun)
{
	if (!device->IsRemoved()) {
		return ReturnLocalizedError(context, ERROR_EJECT_REQUIRED);
	}

	if (!pb_device.vendor().empty() || !pb_device.product().empty() || !pb_device.revision().empty()) {
		return ReturnLocalizedError(context, ERROR_DEVICE_NAME_UPDATE);
	}

	string filename = GetParam(pb_device, "file");
	if (filename.empty()) {
		return ReturnLocalizedError(context, ERROR_MISSING_FILENAME);
	}

	if (dryRun) {
		return true;
	}

	LOGINFO("Insert %sfile '%s' requested into %s ID %d, unit %d", pb_device.protected_() ? "protected " : "",
			filename.c_str(), device->GetType().c_str(), pb_device.id(), pb_device.unit())

	auto disk = dynamic_cast<Disk *>(device);

	if (pb_device.block_size()) {
		if (disk != nullptr&& disk->IsSectorSizeConfigurable()) {
			if (!disk->SetConfiguredSectorSize(pb_device.block_size())) {
				return ReturnLocalizedError(context, ERROR_BLOCK_SIZE, to_string(pb_device.block_size()));
			}
		}
		else {
			return ReturnLocalizedError(context, ERROR_BLOCK_SIZE_NOT_CONFIGURABLE, device->GetType());
		}
	}

	int id;
	int unit;
	Filepath filepath;
	filepath.SetPath(filename.c_str());
	string initial_filename = filepath.GetPath();

	if (FileSupport::GetIdsForReservedFile(filepath, id, unit)) {
		return ReturnLocalizedError(context, ERROR_IMAGE_IN_USE, filename, to_string(id), to_string(unit));
	}

	auto file_support = dynamic_cast<FileSupport *>(device);
	try {
		try {
			file_support->Open(filepath);
		}
		catch(const file_not_found_exception&) {
			// If the file does not exist search for it in the default image folder
			filepath.SetPath((rascsi_image.GetDefaultImageFolder() + "/" + filename).c_str());

			if (FileSupport::GetIdsForReservedFile(filepath, id, unit)) {
				return ReturnLocalizedError(context, ERROR_IMAGE_IN_USE, filename, to_string(id), to_string(unit));
			}

			file_support->Open(filepath);
		}
	}
	catch(const io_exception& e) { //NOSONAR This exception is handled properly
		return ReturnLocalizedError(context, ERROR_FILE_OPEN, initial_filename, e.get_msg());
	}

	file_support->ReserveFile(filepath, device->GetId(), device->GetLun());

	// Only non read-only devices support protect/unprotect.
	// This operation must not be executed before Open() because Open() overrides some settings.
	if (device->IsProtectable() && !device->IsReadOnly()) {
		device->SetProtected(pb_device.protected_());
	}

	if (disk != nullptr) {
		disk->MediumChanged();
	}

	return true;
}

void TerminationHandler(int signum)
{
	Cleanup();

	exit(signum);
}

//---------------------------------------------------------------------------
//
//	Command Processing
//
//---------------------------------------------------------------------------

bool ProcessCmd(const CommandContext& context, const PbDeviceDefinition& pb_device, const PbCommand& command, bool dryRun)
{
	const int id = pb_device.id();
	const int unit = pb_device.unit();
	const PbDeviceType type = pb_device.type();
	const PbOperation operation = command.operation();
	const map<string, string> params = { command.params().begin(), command.params().end() };

	ostringstream s;
	s << (dryRun ? "Validating: " : "Executing: ");
	s << "operation=" << PbOperation_Name(operation);

	if (!params.empty()) {
		s << ", command params=";
		bool isFirst = true;
		for (const auto& [key, value]: params) {
			if (!isFirst) {
				s << ", ";
			}
			isFirst = false;
			string v = key != "token" ? value : "???";
			s << "'" << key << "=" << v << "'";
		}
	}

	s << ", device id=" << id << ", unit=" << unit << ", type=" << PbDeviceType_Name(type);

	if (pb_device.params_size()) {
		s << ", device params=";
		bool isFirst = true;
		for (const auto& [key, value]: pb_device.params()) {
			if (!isFirst) {
				s << ":";
			}
			isFirst = false;
			s << "'" << key << "=" << value << "'";
		}
	}

	s << ", vendor='" << pb_device.vendor() << "', product='" << pb_device.product()
			<< "', revision='" << pb_device.revision()
			<< "', block size=" << pb_device.block_size();
	LOGINFO("%s", s.str().c_str())

	// Check the Controller Number
	if (id < 0) {
		return ReturnLocalizedError(context, ERROR_MISSING_DEVICE_ID);
	}
	if (id >= ControllerManager::DEVICE_MAX) {
		return ReturnLocalizedError(context, ERROR_INVALID_ID, to_string(id), to_string(ControllerManager::DEVICE_MAX - 1));
	}

	if (operation == ATTACH && reserved_ids.find(id) != reserved_ids.end()) {
		return ReturnLocalizedError(context, ERROR_RESERVED_ID, to_string(id));
	}

	// Check the Unit Number
	if (unit < 0 || unit >= AbstractController::LUN_MAX) {
		return ReturnLocalizedError(context, ERROR_INVALID_LUN, to_string(unit), to_string(AbstractController::LUN_MAX - 1));
	}

	if (operation == ATTACH) {
		return Attach(context, pb_device, dryRun);
	}

	// Does the controller exist?
	if (!dryRun && controller_manager.FindController(id) == nullptr) {
		return ReturnLocalizedError(context, ERROR_NON_EXISTING_DEVICE, to_string(id));
	}

	// Does the unit exist?
	PrimaryDevice *device = controller_manager.GetDeviceByIdAndLun(id, unit);
	if (device == nullptr) {
		return ReturnLocalizedError(context, ERROR_NON_EXISTING_UNIT, to_string(id), to_string(unit));
	}

	if (operation == DETACH) {
		return Detach(context, device, dryRun);
	}

	if ((operation == START || operation == STOP) && !device->IsStoppable()) {
		return ReturnLocalizedError(context, ERROR_OPERATION_DENIED_STOPPABLE, device->GetType());
	}

	if ((operation == INSERT || operation == EJECT) && !device->IsRemovable()) {
		return ReturnLocalizedError(context, ERROR_OPERATION_DENIED_REMOVABLE, device->GetType());
	}

	if ((operation == PROTECT || operation == UNPROTECT) && !device->IsProtectable()) {
		return ReturnLocalizedError(context, ERROR_OPERATION_DENIED_PROTECTABLE, device->GetType());
	}
	if ((operation == PROTECT || operation == UNPROTECT) && !device->IsReady()) {
		return ReturnLocalizedError(context, ERROR_OPERATION_DENIED_READY, device->GetType());
	}

	switch (operation) {
		case START:
			if (!dryRun) {
				LOGINFO("Start requested for %s ID %d, unit %d", device->GetType().c_str(), id, unit)

				if (!device->Start()) {
					LOGWARN("Starting %s ID %d, unit %d failed", device->GetType().c_str(), id, unit)
				}
			}
			break;

		case STOP:
			if (!dryRun) {
				LOGINFO("Stop requested for %s ID %d, unit %d", device->GetType().c_str(), id, unit)

				// STOP is idempotent
				device->Stop();
			}
			break;

		case INSERT:
			return Insert(context, pb_device, device, dryRun);

		case EJECT:
			if (!dryRun) {
				LOGINFO("Eject requested for %s ID %d, unit %d", device->GetType().c_str(), id, unit)

				if (!device->Eject(true)) {
					LOGWARN("Ejecting %s ID %d, unit %d failed", device->GetType().c_str(), id, unit)
				}
			}
			break;

		case PROTECT:
			if (!dryRun) {
				LOGINFO("Write protection requested for %s ID %d, unit %d", device->GetType().c_str(), id, unit)

				// PROTECT is idempotent
				device->SetProtected(true);
			}
			break;

		case UNPROTECT:
			if (!dryRun) {
				LOGINFO("Write unprotection requested for %s ID %d, unit %d", device->GetType().c_str(), id, unit)

				// UNPROTECT is idempotent
				device->SetProtected(false);
			}
			break;

		case ATTACH:
		case DETACH:
			// The non dry-run case has been handled before the switch
			assert(dryRun);
			break;

		case CHECK_AUTHENTICATION:
		case NO_OPERATION:
			// Do nothing, just log
			LOGTRACE("Received %s command", PbOperation_Name(operation).c_str())
			break;

		default:
			return ReturnLocalizedError(context, ERROR_OPERATION);
	}

	return true;
}

bool ProcessCmd(const CommandContext& context, const PbCommand& command)
{
	switch (command.operation()) {
		case DETACH_ALL:
			DetachAll();
			return ReturnStatus(context);

		case RESERVE_IDS: {
			const string ids = GetParam(command, "ids");
			string error = SetReservedIds(ids);
			if (!error.empty()) {
				return ReturnStatus(context, false, error);
			}

			return ReturnStatus(context);
		}

		case CREATE_IMAGE:
			return rascsi_image.CreateImage(context, command);

		case DELETE_IMAGE:
			return rascsi_image.DeleteImage(context, command);

		case RENAME_IMAGE:
			return rascsi_image.RenameImage(context, command);

		case COPY_IMAGE:
			return rascsi_image.CopyImage(context, command);

		case PROTECT_IMAGE:
		case UNPROTECT_IMAGE:
			return rascsi_image.SetImagePermissions(context, command);

		default:
			// This is a device-specific command handled below
			break;
	}

	// Remember the list of reserved files, than run the dry run
	const auto reserved_files = FileSupport::GetReservedFiles();
	for (const auto& device : command.devices()) {
		if (!ProcessCmd(context, device, command, true)) {
			// Dry run failed, restore the file list
			FileSupport::SetReservedFiles(reserved_files);
			return false;
		}
	}

	// Restore the list of reserved files before proceeding
	FileSupport::SetReservedFiles(reserved_files);

	if (string result = ValidateLunSetup(command); !result.empty()) {
		return ReturnStatus(context, false, result);
	}

	for (const auto& device : command.devices()) {
		if (!ProcessCmd(context, device, command, false)) {
			return false;
		}
	}

	// ATTACH and DETACH return the device list
	if (context.fd != -1 && (command.operation() == ATTACH || command.operation() == DETACH)) {
		// A new command with an empty device list is required here in order to return data for all devices
		PbCommand command;
		PbResult result;
		rascsi_response.GetDevicesInfo(result, command);
		SerializeMessage(context.fd, result);
		return true;
	}

	return ReturnStatus(context);
}

bool ProcessId(const string& id_spec, int& id, int& unit)
{
	if (size_t separator_pos = id_spec.find(COMPONENT_SEPARATOR); separator_pos == string::npos) {
		if (!GetAsInt(id_spec, id) || id < 0 || id >= 8) {
			cerr << optarg << ": Invalid device ID (0-7)" << endl;
			return false;
		}

		unit = 0;
	}
	else if (!GetAsInt(id_spec.substr(0, separator_pos), id) || id < 0 || id > 7 ||
			!GetAsInt(id_spec.substr(separator_pos + 1), unit) || unit < 0 || unit >= AbstractController::LUN_MAX) {
		cerr << optarg << ": Invalid unit (0-" << (AbstractController::LUN_MAX - 1) << ")" << endl;
		return false;
	}

	return true;
}

void ShutDown(const CommandContext& context, const string& mode) {
	if (mode.empty()) {
		ReturnLocalizedError(context, ERROR_SHUTDOWN_MODE_MISSING);
		return;
	}

	PbResult result;
	result.set_status(true);

	if (mode == "rascsi") {
		LOGINFO("RaSCSI shutdown requested");

		SerializeMessage(context.fd, result);

		TerminationHandler(0);
	}

	// The root user has UID 0
	if (getuid()) {
		ReturnLocalizedError(context, ERROR_SHUTDOWN_PERMISSION);
		return;
	}

	if (mode == "system") {
		LOGINFO("System shutdown requested")

		SerializeMessage(context.fd, result);

		DetachAll();

		if (system("init 0") == -1) {
			LOGERROR("System shutdown failed: %s", strerror(errno))
		}
	}
	else if (mode == "reboot") {
		LOGINFO("System reboot requested")

		SerializeMessage(context.fd, result);

		DetachAll();

		if (system("init 6") == -1) {
			LOGERROR("System reboot failed: %s", strerror(errno))
		}
	}
	else {
		ReturnLocalizedError(context, ERROR_SHUTDOWN_MODE_INVALID);
	}
}

//---------------------------------------------------------------------------
//
//	Argument Parsing
//
//---------------------------------------------------------------------------
bool ParseArgument(int argc, char* argv[], int& port)
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
	int opt;
	while ((opt = getopt(argc, argv, "-Iib:d:n:p:r:t:z:D:F:L:P:R:")) != -1) {
		switch (opt) {
			// The two options below are kind of a compound option with two letters
			case 'i':
			case 'I':
				id = -1;
				unit = -1;
				continue;

			case 'd':
			case 'D': {
				if (!ProcessId(optarg, id, unit)) {
					return false;
				}
				continue;
			}

			case 'b': {
				if (!GetAsInt(optarg, block_size)) {
					cerr << "Invalid block size " << optarg << endl;
					return false;
				}
				continue;
			}

			case 'z':
				locale = optarg;
				continue;

			case 'F': {
				if (string result = rascsi_image.SetDefaultImageFolder(optarg); !result.empty()) {
					cerr << result << endl;
					return false;
				}
				continue;
			}

			case 'L':
				log_level = optarg;
				continue;

			case 'R':
				int depth;
				if (!GetAsInt(optarg, depth) || depth < 0) {
					cerr << "Invalid image file scan depth " << optarg << endl;
					return false;
				}
				rascsi_image.SetDepth(depth);
				continue;

			case 'n':
				name = optarg;
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

			case 'r': {
					string error = SetReservedIds(optarg);
					if (!error.empty()) {
						cerr << error << endl;
						return false;
					}
				}
				continue;

			case 't': {
					string t = optarg;
					transform(t.begin(), t.end(), t.begin(), ::toupper);
					if (!PbDeviceType_Parse(t, &type)) {
						cerr << "Illegal device type '" << optarg << "'" << endl;
						return false;
					}
				}
				continue;

			case 1:
				// Encountered filename
				break;

			default:
				return false;
		}

		if (optopt) {
			return false;
		}

		// Set up the device data
		PbDeviceDefinition *device = command.add_devices();
		device->set_id(id);
		device->set_unit(unit);
		device->set_type(type);
		device->set_block_size(block_size);

		ParseParameters(*device, optarg);

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

	if (!log_level.empty() && !SetLogLevel(log_level)) {
		LOGWARN("Invalid log level '%s'", log_level.c_str())
	}

	// Attach all specified devices
	command.set_operation(ATTACH);

	CommandContext context;
	context.fd = -1;
	context.locale = locale;
	if (!ProcessCmd(context, command)) {
		return false;
	}

	// Display and log the device list
	PbServerInfo server_info;
	rascsi_response.GetDevices(server_info);
	const list<PbDevice>& devices = { server_info.devices_info().devices().begin(), server_info.devices_info().devices().end() };
	const string device_list = ListDevices(devices);
	LogDevices(device_list);
	cout << device_list << endl;

	return true;
}

//---------------------------------------------------------------------------
//
//	Pin the thread to a specific CPU
//
//---------------------------------------------------------------------------
void FixCpu(int cpu)
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

//---------------------------------------------------------------------------
//
//	Monitor Thread
//
//---------------------------------------------------------------------------
static void *MonThread(void *) //NOSONAR The pointer cannot be const void * because of the thread API
{
    // Scheduler Settings
	sched_param schedparam;
	schedparam.sched_priority = 0;
	sched_setscheduler(0, SCHED_IDLE, &schedparam);

	// Set the affinity to a specific processor core
	FixCpu(2);

	// Wait for the execution to start
	while (!running) {
		usleep(1);
	}

	// Set up the monitor socket to receive commands
	listen(monsocket, 1);

	while (true) {
		CommandContext context;
		context.fd = -1;

		try {
			PbCommand command;
			if (!ReadCommand(command, context, monsocket)) {
				continue;
			}

			context.locale = GetParam(command, "locale");
			if (context.locale.empty()) {
				context.locale = "en";
			}

			if (!access_token.empty() && access_token != GetParam(command, "token")) {
				ReturnLocalizedError(context, ERROR_AUTHENTICATION, UNAUTHORIZED);
				continue;
			}

			if (!PbOperation_IsValid(command.operation())) {
				LOGERROR("Received unknown command with operation opcode %d", command.operation())

				ReturnLocalizedError(context, ERROR_OPERATION, UNKNOWN_OPERATION);
				continue;
			}

			LOGTRACE("Received %s command", PbOperation_Name(command.operation()).c_str())

			PbResult result;

			switch(command.operation()) {
				case LOG_LEVEL: {
					string log_level = GetParam(command, "level");
					if (bool status = SetLogLevel(log_level); !status) {
						ReturnLocalizedError(context, ERROR_LOG_LEVEL, log_level);
					}
					else {
						ReturnStatus(context);
					}
					break;
				}

				case DEFAULT_FOLDER: {
					if (string status = rascsi_image.SetDefaultImageFolder(GetParam(command, "folder")); !status.empty()) {
						ReturnStatus(context, false, status);
					}
					else {
						ReturnStatus(context);
					}
					break;
				}

				case DEVICES_INFO: {
					rascsi_response.GetDevicesInfo(result, command);
					SerializeMessage(context.fd, result);
					break;
				}

				case DEVICE_TYPES_INFO: {
					result.set_allocated_device_types_info(rascsi_response.GetDeviceTypesInfo(result));
					SerializeMessage(context.fd, result);
					break;
				}

				case SERVER_INFO: {
					result.set_allocated_server_info(rascsi_response.GetServerInfo(
							result, reserved_ids, current_log_level, GetParam(command, "folder_pattern"),
							GetParam(command, "file_pattern"), rascsi_image.GetDepth()));
					SerializeMessage(context.fd, result);
					break;
				}

				case VERSION_INFO: {
					result.set_allocated_version_info(rascsi_response.GetVersionInfo(result));
					SerializeMessage(context.fd, result);
					break;
				}

				case LOG_LEVEL_INFO: {
					result.set_allocated_log_level_info(rascsi_response.GetLogLevelInfo(result, current_log_level));
					SerializeMessage(context.fd, result);
					break;
				}

				case DEFAULT_IMAGE_FILES_INFO: {
					result.set_allocated_image_files_info(rascsi_response.GetAvailableImages(result,
							GetParam(command, "folder_pattern"), GetParam(command, "file_pattern"),
							rascsi_image.GetDepth()));
					SerializeMessage(context.fd, result);
					break;
				}

				case IMAGE_FILE_INFO: {
					if (string filename = GetParam(command, "file"); filename.empty()) {
						ReturnLocalizedError(context, ERROR_MISSING_FILENAME);
					}
					else {
						auto image_file = make_unique<PbImageFile>();
						bool status = rascsi_response.GetImageFile(image_file.get(), filename);
						if (status) {
							result.set_status(true);
							result.set_allocated_image_file_info(image_file.get());
							SerializeMessage(context.fd, result);
						}
						else {
							ReturnLocalizedError(context, ERROR_IMAGE_FILE_INFO);
						}
					}
					break;
				}

				case NETWORK_INTERFACES_INFO: {
					result.set_allocated_network_interfaces_info(rascsi_response.GetNetworkInterfacesInfo(result));
					SerializeMessage(context.fd, result);
					break;
				}

				case MAPPING_INFO: {
					result.set_allocated_mapping_info(rascsi_response.GetMappingInfo(result));
					SerializeMessage(context.fd, result);
					break;
				}

				case OPERATION_INFO: {
					result.set_allocated_operation_info(rascsi_response.GetOperationInfo(result,
							rascsi_image.GetDepth()));
					SerializeMessage(context.fd, result);
					break;
				}

				case RESERVED_IDS_INFO: {
					result.set_allocated_reserved_ids_info(rascsi_response.GetReservedIds(result, reserved_ids));
					SerializeMessage(context.fd, result);
					break;
				}

				case SHUT_DOWN: {
					ShutDown(context, GetParam(command, "mode"));
					break;
				}

				default: {
					// Wait until we become idle
					while (active) {
						usleep(500 * 1000);
					}

					ProcessCmd(context, command);
					break;
				}
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

//---------------------------------------------------------------------------
//
//	Main processing
//
//---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	BUS::phase_t phase;

	// added setvbuf to override stdout buffering, so logs are written immediately and not when the process exits.
	setvbuf(stdout, nullptr, _IONBF, 0);

	// Output the Banner
	Banner(argc, argv);

	// ParseArgument() requires the bus to have been initialized first, which requires the root user.
	// The -v option should be available for any user, which requires special handling.
	for (int i = 1 ; i < argc; i++) {
		if (!strcasecmp(argv[i], "-v")) {
			cout << rascsi_get_version_string() << endl;
			return 0;
		}
	}

	SetLogLevel("info");

	// Create a thread-safe stdout logger to process the log messages
	auto logger = stdout_color_mt("rascsi stdout logger");

	if (!InitBus()) {
		return EPERM;
	}

	int port = DEFAULT_PORT;
	if (!InitService(port)) {
		return EPERM;
	}

	if (!ParseArgument(argc, argv, port)) {
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
	running = true;

	// Main Loop
	while (running) {
		// Work initialization
		phase = BUS::phase_t::busfree;

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
			// TODO Why sleep for 0 microseconds?
			usleep(0);
			continue;
		}
#endif

        // Wait until BSY is released as there is a possibility for the
        // initiator to assert it while setting the ID (for up to 3 seconds)
		if (bus->GetBSY()) {
			uint32_t now = SysTimer::GetTimerLow();
			while ((SysTimer::GetTimerLow() - now) < 3 * 1000 * 1000) {
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
		BYTE id_data = bus->GetDAT();

		pthread_mutex_lock(&ctrl_mutex);

		// Identify the responsible controller
		shared_ptr<AbstractController> controller = controller_manager.IdentifyController(id_data);
		if (controller != nullptr) {
			initiator_id = controller->ExtractInitiatorId(id_data);

			if (controller->Process(initiator_id) == BUS::phase_t::selection) {
				phase = BUS::phase_t::selection;
			}
		}

		// Return to bus monitoring if the selection phase has not started
		if (phase != BUS::phase_t::selection) {
			pthread_mutex_unlock(&ctrl_mutex);
			continue;
		}

		// Start target device
		active = true;

#ifndef USE_SEL_EVENT_ENABLE
		// Scheduling policy setting (highest priority)
		schparam.sched_priority = sched_get_priority_max(SCHED_FIFO);
		sched_setscheduler(0, SCHED_FIFO, &schparam);
#endif

		// Loop until the bus is free
		while (running) {
			// Target drive
			phase = controller->Process(initiator_id);

			// End when the bus is free
			if (phase == BUS::phase_t::busfree) {
				break;
			}
		}
		pthread_mutex_unlock(&ctrl_mutex);


#ifndef USE_SEL_EVENT_ENABLE
		// Set the scheduling priority back to normal
		schparam.sched_priority = 0;
		sched_setscheduler(0, SCHED_OTHER, &schparam);
#endif

		// End the target travel
		active = false;
	}

	return 0;
}

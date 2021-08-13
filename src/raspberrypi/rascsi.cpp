//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//	[ RaSCSI main ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "filepath.h"
#include "fileio.h"
#include "devices/factory.h"
#include "devices/device.h"
#include "devices/sasihd.h"
#include "devices/scsihd.h"
#include "devices/scsihd_apple.h"
#include "devices/scsihd_nec.h"
#include "devices/scsicd.h"
#include "devices/scsimo.h"
#include "devices/scsi_host_bridge.h"
#include "devices/scsi_daynaport.h"
#include "controllers/scsidev_ctrl.h"
#include "controllers/sasidev_ctrl.h"
#include "gpiobus.h"
#include "exceptions.h"
#include "rascsi_version.h"
#include "rasutil.h"
#include "rascsi_interface.pb.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <spdlog/async.h>
#include <string>
#include <sstream>
#include <iostream>
#include <list>
#include <filesystem>

using namespace std;
using namespace spdlog;
using namespace rascsi_interface;

//---------------------------------------------------------------------------
//
//  Constant declarations
//
//---------------------------------------------------------------------------
#define CtrlMax	8					// Maximum number of SCSI controllers
#define UnitNum	2					// Number of units around controller
#define FPRT(fp, ...) fprintf(fp, __VA_ARGS__ )

//---------------------------------------------------------------------------
//
//	Variable declarations
//
//---------------------------------------------------------------------------
static volatile BOOL running;		// Running flag
static volatile BOOL active;		// Processing flag
vector<SASIDEV *> controllers(CtrlMax);	// Controller
vector<Device *> devices(CtrlMax * UnitNum);	// Disk
GPIOBUS *bus;						// GPIO Bus
int monsocket;						// Monitor Socket
pthread_t monthread;				// Monitor Thread
pthread_mutex_t ctrl_mutex;					// Semaphore for the ctrl array
static void *MonThread(void *param);
list<string> available_log_levels;
string current_log_level;			// Some versions of spdlog do not support get_log_level()
string default_image_folder;
set<string> files_in_use;

//---------------------------------------------------------------------------
//
//	Signal Processing
//
//---------------------------------------------------------------------------
void KillHandler(int sig)
{
	// Stop instruction
	running = FALSE;
}

//---------------------------------------------------------------------------
//
//	Banner Output
//
//---------------------------------------------------------------------------
void Banner(int argc, char* argv[])
{
	FPRT(stdout,"SCSI Target Emulator RaSCSI(*^..^*) ");
	FPRT(stdout,"version %s (%s, %s)\n",
		rascsi_get_version_string(),
		__DATE__,
		__TIME__);
	FPRT(stdout,"Powered by XM6 TypeG Technology / ");
	FPRT(stdout,"Copyright (C) 2016-2020 GIMONS\n");
	FPRT(stdout,"Connect type : %s\n", CONNECT_DESC);

	if ((argc > 1 && strcmp(argv[1], "-h") == 0) ||
		(argc > 1 && strcmp(argv[1], "--help") == 0)){
		FPRT(stdout,"\n");
		FPRT(stdout,"Usage: %s [-IDn FILE] ...\n\n", argv[0]);
		FPRT(stdout," n is SCSI identification number(0-7).\n");
		FPRT(stdout," FILE is disk image file.\n\n");
		FPRT(stdout,"Usage: %s [-HDn FILE] ...\n\n", argv[0]);
		FPRT(stdout," n is X68000 SASI HD number(0-15).\n");
		FPRT(stdout," FILE is disk image file, \"daynaport\", or \"bridge\".\n\n");
		FPRT(stdout," Image type is detected based on file extension.\n");
		FPRT(stdout,"  hdf : SASI HD image (XM6 SASI HD image)\n");
		FPRT(stdout,"  hds : SCSI HD image (Non-removable generic SCSI HD image)\n");
		FPRT(stdout,"  hdr : SCSI HD image (Removable generic SCSI HD image)\n");
		FPRT(stdout,"  hdn : SCSI HD image (NEC GENUINE)\n");
		FPRT(stdout,"  hdi : SCSI HD image (Anex86 HD image)\n");
		FPRT(stdout,"  nhd : SCSI HD image (T98Next HD image)\n");
		FPRT(stdout,"  hda : SCSI HD image (APPLE GENUINE)\n");
		FPRT(stdout,"  mos : SCSI MO image (XM6 SCSI MO image)\n");
		FPRT(stdout,"  iso : SCSI CD image (ISO 9660 image)\n");

		exit(EXIT_SUCCESS);
	}
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------

BOOL InitService(int port)
{
	int result = pthread_mutex_init(&ctrl_mutex,NULL);
	if (result != EXIT_SUCCESS){
		LOGERROR("Unable to create a mutex. Err code: %d", result);
		return FALSE;
	}

	// Create socket for monitor
	struct sockaddr_in server;
	monsocket = socket(PF_INET, SOCK_STREAM, 0);
	memset(&server, 0, sizeof(server));
	server.sin_family = PF_INET;
	server.sin_port   = htons(port);
	server.sin_addr.s_addr = htonl(INADDR_ANY);

	// allow address reuse
	int yes = 1;
	if (setsockopt(monsocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
		return FALSE;
	}

	signal(SIGPIPE, SIG_IGN);

	// Bind
	if (bind(monsocket, (struct sockaddr *)&server,
		sizeof(struct sockaddr_in)) < 0) {
		FPRT(stderr, "Error : Already running?\n");
		return FALSE;
	}

	// Create Monitor Thread
	pthread_create(&monthread, NULL, MonThread, NULL);

	// Interrupt handler settings
	if (signal(SIGINT, KillHandler) == SIG_ERR) {
		return FALSE;
	}
	if (signal(SIGHUP, KillHandler) == SIG_ERR) {
		return FALSE;
	}
	if (signal(SIGTERM, KillHandler) == SIG_ERR) {
		return FALSE;
	}

	running = FALSE;
	active = FALSE;

	return true;
}

bool InitBus()
{
	// GPIOBUS creation
	bus = new GPIOBUS();

	// GPIO Initialization
	if (!bus->Init()) {
		return false;
	}

	// Bus Reset
	bus->Reset();

	return true;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void Cleanup()
{
	// Delete the disks
	for (auto it = devices.begin(); it != devices.end(); ++it) {
		if (*it) {
			delete *it;
			*it = NULL;
		}
	}

	// Delete the Controllers
	for (auto it = controllers.begin(); it != controllers.end(); ++it) {
		if (*it) {
			delete *it;
			*it = NULL;
		}
	}

	// Cleanup the Bus
	if (bus) {
		bus->Cleanup();

		// Discard the GPIOBUS object
		delete bus;
	}

	// Close the monitor socket
	if (monsocket >= 0) {
		close(monsocket);
	}

	pthread_mutex_destroy(&ctrl_mutex);
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void Reset()
{
	// Reset all of the controllers
	for (auto it = controllers.begin(); it != controllers.end(); ++it) {
		if (*it) {
			(*it)->Reset();
		}
	}

	// Reset the bus
	bus->Reset();
}

//---------------------------------------------------------------------------
//
//	Get the list of attached devices
//
//---------------------------------------------------------------------------

void GetImageFile(PbImageFile *image_file, const string& filename)
{
	image_file->set_name(filename);
	if (!filename.empty()) {
		image_file->set_read_only(access(filename.c_str(), W_OK));

		struct stat st;
		stat(image_file->name().c_str(), &st);
		image_file->set_size(st.st_size);
	}
}

const PbDevices GetDevices()
{
	PbDevices pbDevices;

	for (int i = 0; i < CtrlMax * UnitNum; i++) {
		// skip if unit does not exist or null disk
		Device *device = devices[i];
		if (!device) {
			continue;
		}

		PbDevice *pbDevice = pbDevices.add_devices();

		// Initialize ID and unit number
		pbDevice->set_id(i / UnitNum);
		pbDevice->set_unit(i % UnitNum);

		// ID,UNIT,Type,Device Status
		PbDeviceType type = UNDEFINED;
		PbDeviceType_Parse(device->GetType(), &type);
		pbDevice->set_type(type);

		pbDevice->set_read_only(device->IsReadOnly());
		pbDevice->set_protectable(device->IsProtectable());
		pbDevice->set_protected_(device->IsProtectable() && device->IsProtected());
		pbDevice->set_removable(device->IsRemovable());
		pbDevice->set_removed(device->IsRemoved());
		pbDevice->set_lockable(device->IsLockable());
		pbDevice->set_locked(device->IsLocked());

		const FileSupport *fileSupport = dynamic_cast<FileSupport *>(device);
		if (fileSupport) {
			Filepath filepath;
			fileSupport->GetPath(filepath);
			PbImageFile *image_file = new PbImageFile();
			GetImageFile(image_file, device->IsRemovable() && !device->IsReady() ? "" : filepath.GetPath());
			pbDevice->set_allocated_file(image_file);
			pbDevice->set_supports_file(true);
		}
	}

	return pbDevices;
}

//---------------------------------------------------------------------------
//
//	Controller Mapping
//
//---------------------------------------------------------------------------
bool MapController(Device **map)
{
	assert(bus);

	bool status = true;

	// Take ownership of the ctrl data structure
	pthread_mutex_lock(&ctrl_mutex);

	// Replace the changed unit
	for (int i = 0; i < CtrlMax; i++) {
		for (int j = 0; j < UnitNum; j++) {
			int unitno = i * UnitNum + j;
			if (devices[unitno] != map[unitno]) {
				// Check if the original unit exists
				if (devices[unitno]) {
					// Disconnect it from the controller
					if (controllers[i]) {
						controllers[i]->SetUnit(j, NULL);
					}

					// Free the Unit
					delete devices[unitno];
				}

				// Setup a new unit
				devices[unitno] = map[unitno];
			}
		}
	}

	// Reconfigure all of the controllers
	int i = 0;
	for (auto it = controllers.begin(); it != controllers.end(); ++i, ++it) {
		// Examine the unit configuration
		int sasi_num = 0;
		int scsi_num = 0;
		for (int j = 0; j < UnitNum; j++) {
			int unitno = i * UnitNum + j;
			// branch by unit type
			if (devices[unitno]) {
				if (devices[unitno]->IsSASI()) {
					// Drive is SASI, so increment SASI count
					sasi_num++;
				} else {
					// Drive is SCSI, so increment SCSI count
					scsi_num++;
				}
			}

			// Remove the unit
			if (*it) {
				(*it)->SetUnit(j, NULL);
			}
		}

		// If there are no units connected
		if (sasi_num == 0 && scsi_num == 0) {
			if (*it) {
				delete *it;
				*it = NULL;
				continue;
			}
		}

		// Mixture of SCSI and SASI
		if (sasi_num > 0 && scsi_num > 0) {
			status = false;
			continue;
		}

		if (sasi_num > 0) {
			// Only SASI Unit(s)

			// Release the controller if it is not SASI
			if (*it && !(*it)->IsSASI()) {
				delete *it;
				*it = NULL;
			}

			// Create a new SASI controller
			if (!*it) {
				*it = new SASIDEV();
				(*it)->Connect(i, bus);
			}
		} else {
			// Only SCSI Unit(s)

			// Release the controller if it is not SCSI
			if (*it && !(*it)->IsSCSI()) {
				delete *it;
				*it = NULL;
			}

			// Create a new SCSI controller
			if (!*it) {
				*it = new SCSIDEV();
				(*it)->Connect(i, bus);
			}
		}

		// connect all units
		for (int j = 0; j < UnitNum; j++) {
			int unitno = i * UnitNum + j;
			if (devices[unitno]) {
				// Add the unit connection
				(*it)->SetUnit(j, (static_cast<Disk *>(devices[unitno])));
			}
		}
	}
	pthread_mutex_unlock(&ctrl_mutex);

	return status;
}

bool ReturnStatus(int fd, bool status = true, const string msg = "")
{
	if (!status && !msg.empty()) {
		LOGWARN("%s", msg.c_str());
	}

	if (fd == -1) {
		if (!msg.empty()) {
			if (status) {
				FPRT(stderr, "Error: ");
				FPRT(stderr, msg.c_str());
				FPRT(stderr, "\n");
			}
			else {
				FPRT(stdout, msg.c_str());
				FPRT(stderr, "\n");
			}
		}
	}
	else {
		PbResult result;
		result.set_status(status);
		result.set_msg(msg);
		SerializeMessage(fd, result);
	}

	return status;
}

bool ReturnStatus(int fd, bool status, const ostringstream& msg)
{
	return ReturnStatus(fd, status, msg.str());
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

	return true;
}

void LogDevices(const string& device_list)
{
	stringstream ss(device_list);
	string line;

	while (getline(ss, line, '\n')) {
		LOGINFO("%s", line.c_str());
	}
}

void GetAvailableLogLevels(PbServerInfo& serverInfo)
{
	for (auto it = available_log_levels.begin(); it != available_log_levels.end(); ++it) {
		serverInfo.add_available_log_levels(*it);
	}
}

void GetAvailableImages(PbServerInfo& serverInfo)
{
	if (access(default_image_folder.c_str(), F_OK) != -1) {
		for (const auto& entry : filesystem::directory_iterator(default_image_folder)) {
			if (entry.is_regular_file()) {
				PbImageFile *image_file = serverInfo.add_available_image_files();
				GetImageFile(image_file, entry.path().filename());
			}
		}
	}
}

void SetDeviceName(Device *device, const string& name)
{
	size_t productSeparatorPos = name.find(':');
	if (productSeparatorPos != string::npos) {
		device->SetVendor(name.substr(0, productSeparatorPos));

		string remaining = name.substr(productSeparatorPos + 1);
		size_t revisionSeparatorPos = remaining.find(':');
		if (revisionSeparatorPos != string::npos) {
			device->SetProduct(remaining.substr(0, revisionSeparatorPos));
			device->SetRevision(remaining.substr(revisionSeparatorPos + 1));
			return;
		}
	}

	ostringstream error;
	error << "Wrong device name format: '" << name << "', must be VENDOR:PRODUCT:REVISION";
	throw illegalargumentexception(error.str());
}

//---------------------------------------------------------------------------
//
//	Command Processing
//
//---------------------------------------------------------------------------

bool ProcessCmd(int fd, const PbDeviceDefinition& pbDevice, const PbOperation cmd, const string& params, bool dryRun)
{
	Filepath filepath;
	Device *device;
	ostringstream error;

	int id = pbDevice.id();
	int unit = pbDevice.unit();
	string filename = pbDevice.file();
	PbDeviceType type = pbDevice.type();

	ostringstream s;
	s << (dryRun ? "Validating: " : "Executing: ");
	s << "cmd=" << PbOperation_Name(cmd) << ", id=" << id << ", unit=" << unit << ", type=" << PbDeviceType_Name(type)
			<< ", filename='" << filename << "', device name='" << pbDevice.name() << "', params='" << params << "'";
	LOGINFO("%s", s.str().c_str());

	// Check the Controller Number
	if (id < 0 || id >= CtrlMax) {
		error << "Invalid ID " << id << " (0-" << CtrlMax - 1 << ")";
		return ReturnStatus(fd, false, error);
	}

	// Check the Unit Number
	if (unit < 0 || unit >= UnitNum) {
		error << "Invalid unit " << unit << " (0-" << UnitNum - 1 << ")";
		return ReturnStatus(fd, false, error);
	}

	// Copy the devices
	Device *map[CtrlMax * UnitNum];
	for (int i = 0; i < CtrlMax * UnitNum; i++) {
		map[i] = devices[i];
	}

	if (cmd == ATTACH) {
		if (map[id * UnitNum + unit]) {
			error << "Duplicate ID " << id;
			return ReturnStatus(fd, false, error);
		}

		string ext;
		int len = filename.length();
		if (len > 4 && filename[len - 4] == '.') {
			ext = filename.substr(len - 3);
		}

		// Create a new device, based upon type or file extension
		device = DeviceFactory::CreateDevice(type, ext);
		if (!device) {
			error << "Received a command for an invalid device type: " << PbDeviceType_Name(type);
			return ReturnStatus(fd, false, error);
		}

		if (!pbDevice.name().empty()) {
			try {
				SetDeviceName(device, pbDevice.name());
			}
			catch(const illegalargumentexception& e) {
				return ReturnStatus(fd, false, e.getmsg());
			}
		}

		device->SetId(id);
		device->SetLun(unit);

		if (pbDevice.protected_() && device->IsProtectable()) {
			device->SetProtected(true);
		}

		FileSupport *fileSupport = dynamic_cast<FileSupport *>(device);

		// File check (type is HD, for removable media drives, CD and MO the medium (=file) may be inserted later)
		if (fileSupport && !pbDevice.removable() && filename.empty()) {
			free(device);

			error << "Device type " << PbDeviceType_Name(type) << " requires a filename";
			return ReturnStatus(fd, false, error);
		}

		// drive checks files
		if (fileSupport && !filename.empty()) {
			// Set the Path
			filepath.SetPath(filename.c_str());

			// Open the file path
			try {
				fileSupport->Open(filepath);
			}
			catch(const ioexception& e) {
				// If the file does not exist search for it in the default image folder
				string default_file = default_image_folder + "/" + filename;
				filepath.SetPath(default_file.c_str());
				try {
					fileSupport->Open(filepath);
				}
				catch(const ioexception&) {
					delete device;

					error << "Tried to open an invalid file '" << filename << "': " << e.getmsg();
					return ReturnStatus(fd, false, error);
				}
			}

			if (!dryRun) {
				if (files_in_use.find(filepath.GetPath()) != files_in_use.end()) {
					error << "Image file '" << filename << "' is already in use";
					return ReturnStatus(fd, false, error);
				}

				files_in_use.insert(filepath.GetPath());
			}
		}

		if (!dryRun) {
			// Replace with the newly created unit
			map[id * UnitNum + unit] = device;

			// Re-map the controller
			bool status = MapController(map);
			if (status) {
				LOGINFO("Added new %s device, ID: %d unit: %d", device->GetType().c_str(), id, unit);
				return true;
			}

			return ReturnStatus(fd, false, "SASI and SCSI can't be mixed");
		}
	}

	// Does the controller exist?
	if (!dryRun && controllers[id] == NULL) {
		error << "Received a command for invalid ID " << id;
		return ReturnStatus(fd, false, error);
	}

	// Does the unit exist?
	device = devices[id * UnitNum + unit];
	if (!dryRun && device == NULL) {
		error << "Received a command for invalid ID " << id << ", unit " << unit;
		return ReturnStatus(fd, false, error);
	}

	FileSupport *fileSupport = dynamic_cast<FileSupport *>(device);

	if (!dryRun && cmd == DETACH) {
		// Free the existing unit
		map[id * UnitNum + unit] = NULL;

		if (fileSupport) {
			Filepath filepath;
			fileSupport->GetPath(filepath);
			files_in_use.erase(filepath.GetPath());
		}

		// Re-map the controller
		bool status = MapController(map);
		if (status) {
	        LOGINFO("Disconnected %s device, ID: %d unit: %d", device->GetType().c_str(), id, unit);
			return true;
		}

		return ReturnStatus(fd, false, "SASI and SCSI can't be mixed");
	}

	// Only MOs or CDs may be inserted/ejected, only MOs, CDs or hard disks may be protected
	if ((cmd == INSERT || cmd == EJECT) && !device->IsRemovable()) {
		error << PbOperation_Name(cmd) << " operation denied (Device type " << device->GetType() << " isn't removable)";
		return ReturnStatus(fd, false, error);
	}

	if ((cmd == PROTECT || cmd == UNPROTECT) && (!device->IsProtectable() || device->IsReadOnly())) {
		error << PbOperation_Name(cmd) << " operation denied (Device type " << device->GetType() << " isn't protectable)";
		return ReturnStatus(fd, false, error);
	}

	if (dryRun) {
		return true;
	}

	switch (cmd) {
		case INSERT:
			if (params.empty()) {
				return ReturnStatus(fd, false, "Missing filename");
			}

			filepath.SetPath(params.c_str());
			LOGINFO("Insert file '%s' requested into %s ID: %d unit: %d", params.c_str(), device->GetType().c_str(), id, unit);

			try {
				fileSupport->Open(filepath);
			}
			catch(const ioexception& e) {
				// If the file does not exist search for it in the default image folder
				string default_file = default_image_folder + "/" + params;
				filepath.SetPath(default_file.c_str());
				try {
					fileSupport->Open(filepath);
				}
				catch(const ioexception&) {
					error << "Tried to open an invalid file '" << params << "': " << e.getmsg();
					LOGWARN("%s", error.str().c_str());
					return ReturnStatus(fd, false, error);
				}
			}
			break;

		case EJECT:
			LOGINFO("Eject requested for %s ID: %d unit: %d", device->GetType().c_str(), id, unit);
			device->Eject(true);
			break;

		case PROTECT:
			LOGINFO("Write protection requested for %s ID: %d unit: %d", device->GetType().c_str(), id, unit);
			device->SetProtected(true);
			break;

		case UNPROTECT:
			LOGINFO("Write unprotection requested for %s ID: %d unit: %d", device->GetType().c_str(), id, unit);
			device->SetProtected(false);
			break;

		default:
			error << "Received unknown command: " << PbOperation_Name(cmd);
			return ReturnStatus(fd, false, error);
	}

	return true;
}

bool ProcessCmd(int fd, const PbCommand& command)
{
	// Dry run first
	for (int i = 0; i < command.devices().devices_size(); i++) {
		if (!ProcessCmd(fd, command.devices().devices(i), command.cmd(), command.params(), true)) {
			return false;
		}
	}

	// Execute
	for (int i = 0; i < command.devices().devices_size(); i++) {
		if (!ProcessCmd(fd, command.devices().devices(i), command.cmd(), command.params(), false)) {
			return false;
		}
	}

	return ReturnStatus(fd);
}

//---------------------------------------------------------------------------
//
//	Argument Parsing
//
//---------------------------------------------------------------------------
bool ParseArgument(int argc, char* argv[], int& port)
{
	PbDeviceDefinitions devices;
	int id = -1;
	bool is_sasi = false;
	int max_id = 7;
	PbDeviceType type = UNDEFINED;
	string name;
	string log_level = "trace";

	int opt;
	while ((opt = getopt(argc, argv, "-IiHhG:g:D:d:N:n:T:t:P:p:f:Vv")) != -1) {
		switch (tolower(opt)) {
			case 'i':
				is_sasi = false;
				max_id = 7;
				id = -1;
				continue;

			case 'h':
				is_sasi = true;
				max_id = 15;
				id = -1;
				continue;

			case 'g':
				log_level = optarg;
				continue;

			case 'd': {
				char* end;
				id = strtol(optarg, &end, 10);
				if (*end || id < 0 || max_id < id) {
					cerr << optarg << ": invalid " << (is_sasi ? "HD" : "ID") << " (0-" << max_id << ")" << endl;
					return false;
				}
				continue;
			}

			case 'p':
				port = atoi(optarg);
				if (port <= 0 || port > 65535) {
					cerr << "Invalid port " << optarg << ", port must be between 1 and 65535" << endl;
					return false;
				}
				continue;

			case 'f':
				struct stat folder_stat;
				stat(optarg, &folder_stat);
				if (!S_ISDIR(folder_stat.st_mode) || access(optarg, F_OK) == -1) {
					cerr << "Default image folder '" << optarg << "' is not accessible" << endl;
					return false;
				}

				default_image_folder = optarg;
				continue;

			case 'n':
				name = optarg;
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

			case 'v':
				cout << rascsi_get_version_string() << endl;
				exit(EXIT_SUCCESS);
				break;

			default:
				return false;

			case 1:
				// Encountered filename
				break;
		}

		int unit = 0;
		if (is_sasi) {
			unit = id % UnitNum;
			id /= UnitNum;
		}

		// Set up the device data
		PbDeviceDefinition *device = devices.add_devices();
		device->set_id(id);
		device->set_unit(unit);
		device->set_type(type);
		device->set_name(name);
		device->set_file(optarg);

		id = -1;
		type = UNDEFINED;
	}

	if (!SetLogLevel(log_level)) {
		LOGWARN("Invalid log level '%s'", log_level.c_str());
	}

	// Attach all specified devices
	PbCommand command;
	command.set_cmd(ATTACH);
	command.set_allocated_devices(new PbDeviceDefinitions(devices));

	if (!ProcessCmd(-1, command)) {
		return false;
	}

	// Display and log the device list
	const string deviceList = ListDevices(GetDevices());
	cout << deviceList << endl;
	LogDevices(deviceList);

	return true;
}

//---------------------------------------------------------------------------
//
//	Pin the thread to a specific CPU
//
//---------------------------------------------------------------------------
void FixCpu(int cpu)
{
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
}

//---------------------------------------------------------------------------
//
//	Monitor Thread
//
//---------------------------------------------------------------------------
static void *MonThread(void *param)
{
    // Scheduler Settings
	struct sched_param schedparam;
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
		int fd = -1;

		try {
			// Wait for connection
			struct sockaddr_in client;
			socklen_t socklen = sizeof(client);
			memset(&client, 0, socklen);
			fd = accept(monsocket, (struct sockaddr*)&client, &socklen);
			if (fd < 0) {
				throw ioexception("accept() failed");
			}

			// Fetch the command
			PbCommand command;
			DeserializeMessage(fd, command);

			switch(command.cmd()) {
				case LIST: {
					const PbDevices devices = GetDevices();
					SerializeMessage(fd, devices);
					LogDevices(ListDevices(devices));
					break;
				}

				case LOG_LEVEL: {
					bool status = SetLogLevel(command.params());
					if (!status) {
						ostringstream error;
						error << "Invalid log level: " << command.params();
						ReturnStatus(fd, false, error);
					}
					else {
						ReturnStatus(fd);
					}
					break;
				}

				case SERVER_INFO: {
					PbServerInfo serverInfo;
					serverInfo.set_rascsi_version(rascsi_get_version_string());
					GetAvailableLogLevels(serverInfo);
					serverInfo.set_current_log_level(current_log_level);
					serverInfo.set_default_image_folder(default_image_folder);
					GetAvailableImages(serverInfo);
					serverInfo.set_allocated_devices(new PbDevices(GetDevices()));
					SerializeMessage(fd, serverInfo);
					break;
				}

				default: {
					// Wait until we become idle
					while (active) {
						usleep(500 * 1000);
					}

					ProcessCmd(fd, command);
					break;
				}
			}
		}
		catch(const ioexception& e) {
			LOGWARN("%s", e.getmsg().c_str());

			// Fall through
		}

		if (fd >= 0) {
			close(fd);
		}
	}

	return NULL;
}

//---------------------------------------------------------------------------
//
//	Main processing
//
//---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	int actid;
	DWORD now;
	BUS::phase_t phase;
	BYTE data;
	// added setvbuf to override stdout buffering, so logs are written immediately and not when the process exits.
	setvbuf(stdout, NULL, _IONBF, 0);
	struct sched_param schparam;

	available_log_levels.push_back("trace");
	available_log_levels.push_back("debug");
	available_log_levels.push_back("info");
	available_log_levels.push_back("warn");
	available_log_levels.push_back("err");
	available_log_levels.push_back("critical");
	available_log_levels.push_back("off");
	SetLogLevel("trace");

	// Create a thread-safe stdout logger to process the log messages
	auto logger = stdout_color_mt("rascsi stdout logger");

	// ~/images is the default folder for device image file
	const passwd *passwd = getpwuid(getuid());
	default_image_folder = passwd->pw_dir;
	default_image_folder += "/images";

	// Output the Banner
	Banner(argc, argv);

	int ret = 0;
	int port = 6868;

	if (!InitBus()) {
		ret = EPERM;
		goto init_exit;
	}

	if (!ParseArgument(argc, argv, port)) {
		ret = EINVAL;
		goto err_exit;
	}

	if (!InitService(port)) {
		ret = EPERM;
		goto init_exit;
	}

	// Reset
	Reset();

    // Set the affinity to a specific processor core
	FixCpu(3);

#ifdef USE_SEL_EVENT_ENABLE
	// Scheduling policy setting (highest priority)
	schparam.sched_priority = sched_get_priority_max(SCHED_FIFO);
	sched_setscheduler(0, SCHED_FIFO, &schparam);
#endif	// USE_SEL_EVENT_ENABLE

	// Start execution
	running = TRUE;

	// Main Loop
	while (running) {
		// Work initialization
		actid = -1;
		phase = BUS::busfree;

#ifdef USE_SEL_EVENT_ENABLE
		// SEL signal polling
		if (bus->PollSelectEvent() < 0) {
			// Stop on interrupt
			if (errno == EINTR) {
				break;
			}
			continue;
		}

		// Get the bus
		bus->Aquire();
#else
		bus->Aquire();
		if (!bus->GetSEL()) {
			usleep(0);
			continue;
		}
#endif	// USE_SEL_EVENT_ENABLE

        // Wait until BSY is released as there is a possibility for the
        // initiator to assert it while setting the ID (for up to 3 seconds)
		if (bus->GetBSY()) {
			now = SysTimer::GetTimerLow();
			while ((SysTimer::GetTimerLow() - now) < 3 * 1000 * 1000) {
				bus->Aquire();
				if (!bus->GetBSY()) {
					break;
				}
			}
		}

		// Stop because it the bus is busy or another device responded
		if (bus->GetBSY() || !bus->GetSEL()) {
			continue;
		}

		pthread_mutex_lock(&ctrl_mutex);

		// Notify all controllers
		data = bus->GetDAT();
		int i = 0;
		for (auto it = controllers.begin(); it != controllers.end(); ++i, ++it) {
			if (!*it || (data & (1 << i)) == 0) {
				continue;
			}

			// Find the target that has moved to the selection phase
			if ((*it)->Process() == BUS::selection) {
				// Get the target ID
				actid = i;

				// Bus Selection phase
				phase = BUS::selection;
				break;
			}
		}

		// Return to bus monitoring if the selection phase has not started
		if (phase != BUS::selection) {
			pthread_mutex_unlock(&ctrl_mutex);
			continue;
		}

		// Start target device
		active = TRUE;

#ifndef USE_SEL_EVENT_ENABLE
		// Scheduling policy setting (highest priority)
		schparam.sched_priority = sched_get_priority_max(SCHED_FIFO);
		sched_setscheduler(0, SCHED_FIFO, &schparam);
#endif	// USE_SEL_EVENT_ENABLE

		// Loop until the bus is free
		while (running) {
			// Target drive
			phase = controllers[actid]->Process();

			// End when the bus is free
			if (phase == BUS::busfree) {
				break;
			}
		}
		pthread_mutex_unlock(&ctrl_mutex);


#ifndef USE_SEL_EVENT_ENABLE
		// Set the scheduling priority back to normal
		schparam.sched_priority = 0;
		sched_setscheduler(0, SCHED_OTHER, &schparam);
#endif	// USE_SEL_EVENT_ENABLE

		// End the target travel
		active = FALSE;
	}

err_exit:
	// Cleanup
	Cleanup();

init_exit:
	return ret;
}

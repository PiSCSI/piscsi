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
#include "devices/disk.h"
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
SASIDEV *ctrl[CtrlMax];				// Controller
Disk *disk[CtrlMax * UnitNum];		// Disk
GPIOBUS *bus;						// GPIO Bus
int monsocket;						// Monitor Socket
pthread_t monthread;				// Monitor Thread
pthread_mutex_t ctrl_mutex;					// Semaphore for the ctrl array
static void *MonThread(void *param);
list<string> available_log_levels;
string current_log_level;			// Some versions of spdlog do not support get_log_level()
string default_image_folder = "/home/pi/images";
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

		exit(0);
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
	if(result != EXIT_SUCCESS){
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
	if (setsockopt(
		monsocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0){
		return FALSE;
	}

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

void InitDisks()
{
	// Controller initialization
	for (int i = 0; i < CtrlMax; i++) {
		ctrl[i] = NULL;
	}

	// Disk Initialization
	for (int i = 0; i < CtrlMax; i++) {
		disk[i] = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void Cleanup()
{
	// Delete the disks
	for (int i = 0; i < CtrlMax * UnitNum; i++) {
		if (disk[i]) {
			delete disk[i];
			disk[i] = NULL;
		}
	}

	// Delete the Controllers
	for (int i = 0; i < CtrlMax; i++) {
		if (ctrl[i]) {
			delete ctrl[i];
			ctrl[i] = NULL;
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
	for (int i = 0; i < CtrlMax; i++) {
		if (ctrl[i]) {
			ctrl[i]->Reset();
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
const PbDevices GetDevices() {
	PbDevices devices;

	for (int i = 0; i < CtrlMax * UnitNum; i++) {
		// skip if unit does not exist or null disk
		Disk *pUnit = disk[i];
		if (!pUnit) {
			continue;
		}

		PbDevice *device = devices.add_devices();

		// Initialize ID and unit number
		device->set_id(i / UnitNum);
		device->set_unit(i % UnitNum);

		// ID,UNIT,Type,Device Status
		PbDeviceType type;
		PbDeviceType_Parse(pUnit->GetID(), &type);
		device->set_type(type);
		PbImageFile image_file;

		// mount status output
		if (pUnit->IsBridge()) {
			image_file.set_filename("X68000 HOST BRIDGE");
		} else if (pUnit->IsDaynaPort()) {
			image_file.set_filename("DaynaPort SCSI/Link");
		} else {
			Filepath filepath;
			pUnit->GetPath(filepath);
			image_file.set_filename(pUnit->IsRemovable() && !pUnit->IsReady() ? "" : filepath.GetPath());
			image_file.set_read_only(access(filepath.GetPath(), W_OK));
			if (!image_file.filename().empty()) {
				struct stat st;
				stat(image_file.filename().c_str(), &st);
				image_file.set_size(st.st_size);
			}
		}

		device->set_protectable(pUnit->IsProtectable());
		device->set_protected_(pUnit->IsProtectable() && pUnit->IsWriteP());
		device->set_removable(pUnit->IsRemovable());
		device->set_removed(pUnit->IsRemoved());
		device->set_lockable(pUnit->IsLockable());
		device->set_locked(pUnit->IsLocked());
		device->set_supports_file(pUnit->SupportsFile());
		device->set_allocated_image_file(new PbImageFile(image_file));

		// Write protection status
		if (pUnit->IsRemovable() && pUnit->IsReady() && pUnit->IsWriteP()) {
			device->set_read_only(true);
		}
	}

	return devices;
}

//---------------------------------------------------------------------------
//
//	Controller Mapping
//
//---------------------------------------------------------------------------
bool MapController(Disk **map)
{
	bool status = true;

	// Take ownership of the ctrl data structure
	pthread_mutex_lock(&ctrl_mutex);

	// Replace the changed unit
	for (int i = 0; i < CtrlMax; i++) {
		for (int j = 0; j < UnitNum; j++) {
			int unitno = i * UnitNum + j;
			if (disk[unitno] != map[unitno]) {
				// Check if the original unit exists
				if (disk[unitno]) {
					// Disconnect it from the controller
					if (ctrl[i]) {
						ctrl[i]->SetUnit(j, NULL);
					}

					// Free the Unit
					delete disk[unitno];
				}

				// Setup a new unit
				disk[unitno] = map[unitno];
			}
		}
	}

	// Reconfigure all of the controllers
	for (int i = 0; i < CtrlMax; i++) {
		// Examine the unit configuration
		int sasi_num = 0;
		int scsi_num = 0;
		for (int j = 0; j < UnitNum; j++) {
			int unitno = i * UnitNum + j;
			// branch by unit type
			if (disk[unitno]) {
				if (disk[unitno]->IsSASI()) {
					// Drive is SASI, so increment SASI count
					sasi_num++;
				} else {
					// Drive is SCSI, so increment SCSI count
					scsi_num++;
				}
			}

			// Remove the unit
			if (ctrl[i]) {
				ctrl[i]->SetUnit(j, NULL);
			}
		}

		// If there are no units connected
		if (sasi_num == 0 && scsi_num == 0) {
			if (ctrl[i]) {
				delete ctrl[i];
				ctrl[i] = NULL;
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
			if (ctrl[i] && !ctrl[i]->IsSASI()) {
				delete ctrl[i];
				ctrl[i] = NULL;
			}

			// Create a new SASI controller
			if (!ctrl[i]) {
				ctrl[i] = new SASIDEV();
				ctrl[i]->Connect(i, bus);
			}
		} else {
			// Only SCSI Unit(s)

			// Release the controller if it is not SCSI
			if (ctrl[i] && !ctrl[i]->IsSCSI()) {
				delete ctrl[i];
				ctrl[i] = NULL;
			}

			// Create a new SCSI controller
			if (!ctrl[i]) {
				ctrl[i] = new SCSIDEV();
				ctrl[i]->Connect(i, bus);
			}
		}

		// connect all units
		for (int j = 0; j < UnitNum; j++) {
			int unitno = i * UnitNum + j;
			if (disk[unitno]) {
				// Add the unit connection
				ctrl[i]->SetUnit(j, disk[unitno]);
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
			FPRT(stderr, "Error: ");
			FPRT(stderr, msg.c_str());
			FPRT(stderr, "\n");
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
				serverInfo.add_available_image_files(entry.path().filename());
			}
		}
	}
}

//---------------------------------------------------------------------------
//
//	Command Processing
//
//---------------------------------------------------------------------------
bool ProcessCmd(int fd, const PbCommand& command)
{
	Filepath filepath;
	Disk *pUnit;
	ostringstream error;

	PbOperation cmd = command.cmd();
	PbDevice device = command.device();
	int id = device.id();
	int unit = device.unit();
	string filename = device.image_file().filename();
	PbDeviceType type = device.type();
	string params = command.params().c_str();

	ostringstream s;
	s << "Processing: cmd=" << PbOperation_Name(cmd) << ", id=" << id << ", unit=" << unit << ", type=" << PbDeviceType_Name(type)
			<< ", file='" << filename << "', params='" << params << "'";
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

	// Copy the Unit List
	Disk *map[CtrlMax * UnitNum];
	memcpy(map, disk, sizeof(disk));

	// Connect Command
	if (cmd == ATTACH) {
		if (map[id * UnitNum + unit]) {
			error << "Duplicate ID " << id;
			return ReturnStatus(fd, false, error);
		}

		// Try to extract the file type from the filename. Convention: "filename:type".
		size_t separatorPos = filename.find(':');
		if (separatorPos != string::npos) {
			string t = filename.substr(separatorPos + 1);
			transform(t.begin(), t.end(), t.begin(), ::toupper);

			if (!PbDeviceType_Parse(t, &type)) {
				error << "Invalid device type " << t;
				return ReturnStatus(fd, false, error);
			}

			filename = filename.substr(0, separatorPos);
		}

		string ext;
		int len = filename.size();
		if (len > 4 && filename[len - 4] == '.') {
			ext = filename.substr(len - 3);
		}

		// If no type was specified try to derive the file type from the extension
		if (type == UNDEFINED) {
			if (ext == "hdf") {
				type = SAHD;
			}
			else if (ext == "hds" || ext == "hdn" || ext == "hdi" || ext == "nhd" || ext == "hda") {
				type = SCHD;
			}
			else if (ext == "hdr") {
				type = SCRM;
			} else if (ext == "mos") {
				type = SCMO;
			} else if (ext == "iso") {
				type = SCCD;
			}
		}

		// File check (type is HD, for CD and MO the medium (=file) may be inserted later)
		if ((type == SAHD || type == SCHD || type == SCRM) && filename.empty()) {
			return ReturnStatus(fd, false, "Missing filename");
		}

		// Create a new drive, based upon type
		pUnit = NULL;
		switch (type) {
			case SAHD:		// HDF
				pUnit = new SASIHD();
				break;

			case SCHD:		// HDS/HDN/HDI/NHD/HDA
				if (ext == "hdn" || ext == "hdi" || ext == "nhd") {
					pUnit = new SCSIHD_NEC();
				} else if (ext == "hda") {
					pUnit = new SCSIHD_APPLE();
				} else {
					pUnit = new SCSIHD(false);
				}
				break;

			case SCRM:
				pUnit = new SCSIHD(true);
				break;

			case SCMO:
				pUnit = new SCSIMO();
				break;

			case SCCD:
				pUnit = new SCSICD();
				break;

			case SCBR:
				pUnit = new SCSIBR();
				break;

			case SCDP:
				pUnit = new SCSIDaynaPort();
				break;

			default:
				error << "Received a command for an invalid drive type: " << PbDeviceType_Name(type);
				return ReturnStatus(fd, false, error);
		}

		// drive checks files
		if (type != SCBR && type != SCDP && !command.params().empty()) {
			// Set the Path
			filepath.SetPath(filename.c_str());

			// Open the file path
			try {
				pUnit->Open(filepath);
			}
			catch(const ioexception& e) {
				// If the file does not exist search for it in the default image folder
				string default_file = default_image_folder + "/" + filename;
				filepath.SetPath(default_file.c_str());
				try {
					pUnit->Open(filepath);
				}
				catch(const ioexception&) {
					delete pUnit;

					error << "Tried to open an invalid file '" << filename << "': " << e.getmsg();
					return ReturnStatus(fd, false, error);
				}
			}

			if (files_in_use.find(filepath.GetPath()) != files_in_use.end()) {
				error << "Image file '" << filename << "' is already in use";
				return ReturnStatus(fd, false, error);
			}

			files_in_use.insert(filepath.GetPath());
		}

		// Set the cache to write-through
		pUnit->SetCacheWB(FALSE);

		// Replace with the newly created unit
		map[id * UnitNum + unit] = pUnit;

		// Re-map the controller
		bool status = MapController(map);
		if (status) {
	        LOGINFO("Added new %s device. ID: %d UN: %d", pUnit->GetID().c_str(), id, unit);
		}

		return ReturnStatus(fd, status, status ? "" : "SASI and SCSI can't be mixed");
	}

	// Does the controller exist?
	if (ctrl[id] == NULL) {
		error << "Received a command for invalid ID " << id;
		return ReturnStatus(fd, false, error);
	}

	// Does the unit exist?
	pUnit = disk[id * UnitNum + unit];
	if (pUnit == NULL) {
		error << "Received a command for invalid ID " << id << ", unit " << unit;
		return ReturnStatus(fd, false, error);
	}

	// Disconnect Command
	if (cmd == DETACH) {
		LOGINFO("Disconnect %s at ID: %d UN: %d", pUnit->GetID().c_str(), id, unit);

		// Free the existing unit
		map[id * UnitNum + unit] = NULL;

		Filepath filepath;
		pUnit->GetPath(filepath);
		files_in_use.erase(filepath.GetPath());

		// Re-map the controller
		bool status = MapController(map);

		return ReturnStatus(fd, status, status ? "" : "SASI and SCSI can't be mixed");
	}

	// Only MOs or CDs may be inserted/ejected, only MOs, CDs or hard disks may be protected
	if ((cmd == INSERT || cmd == EJECT) && !pUnit->IsRemovable()) {
		error << PbOperation_Name(cmd) << " operation denied (Device type " << pUnit->GetID() << " isn't removable)";
		return ReturnStatus(fd, false, error);
	}

	if ((cmd == PROTECT || cmd == UNPROTECT) && (!pUnit->IsProtectable() || pUnit->IsReadOnly())) {
		error << PbOperation_Name(cmd) << " operation denied (Device type " << pUnit->GetID() << " isn't protectable)";
		return ReturnStatus(fd, false, error);
	}

	switch (cmd) {
		case INSERT:
			if (params.empty()) {
				return ReturnStatus(fd, false, "Missing filename");
			}

			filepath.SetPath(params.c_str());
			LOGINFO("Insert file '%s' requested into %s ID: %d UN: %d", params.c_str(), pUnit->GetID().c_str(), id, unit);

			try {
				pUnit->Open(filepath);
			}
			catch(const ioexception& e) {
				// If the file does not exist search for it in the default image folder
				string default_file = default_image_folder + "/" + params;
				filepath.SetPath(default_file.c_str());
				try {
					pUnit->Open(filepath);
				}
				catch(const ioexception&) {
					error << "Tried to open an invalid file '" << params << "': " << e.getmsg();
					LOGWARN("%s", error.str().c_str());
					return ReturnStatus(fd, false, error);
				}
			}
			break;

		case EJECT:
			LOGINFO("Eject requested for %s ID: %d UN: %d", pUnit->GetID().c_str(), id, unit);
			pUnit->Eject(true);
			break;

		case PROTECT:
			LOGINFO("Write protection requested for %s ID: %d UN: %d", pUnit->GetID().c_str(), id, unit);
			pUnit->WriteP(true);
			break;

		case UNPROTECT:
			LOGINFO("Write unprotection requested for %s ID: %d UN: %d", pUnit->GetID().c_str(), id, unit);
			pUnit->WriteP(false);
			break;

		default:
			error << "Received unknown command: " << PbOperation_Name(cmd);
			return ReturnStatus(fd, false, error);
	}

	return ReturnStatus(fd);
}

bool has_suffix(const string& filename, const string& suffix) {
    return filename.size() >= suffix.size() && !filename.compare(filename.size() - suffix.size(), suffix.size(), suffix);
}

//---------------------------------------------------------------------------
//
//	Argument Parsing
//
//---------------------------------------------------------------------------
bool ParseArgument(int argc, char* argv[], int& port)
{
	int id = -1;
	bool is_sasi = false;
	int max_id = 7;
	string log_level = "trace";

	int opt;
	while ((opt = getopt(argc, argv, "-IiHhG:g:D:d:P:p:f:Vv")) != -1) {
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

			case 'v':
				cout << rascsi_get_version_string() << endl;
				exit(0);
				break;

			default:
				return false;

			case 1:
				break;
		}

		int unit = 0;
		if (is_sasi) {
			unit = id % UnitNum;
			id /= UnitNum;
		}

		// Execute the command
		PbDevice device;
		device.set_id(id);
		device.set_unit(unit);
		PbImageFile image_file;
		image_file.set_filename(optarg);
		device.set_allocated_image_file(new PbImageFile(image_file));
		PbCommand command;
		command.set_allocated_device(new PbDevice(device));
		command.set_cmd(ATTACH);
		command.set_params(optarg);
		if (!ProcessCmd(-1, command)) {
			return false;
		}

		id = -1;
	}

	if (!SetLogLevel(log_level)) {
		LOGWARN("Invalid log level '%s'", log_level.c_str());
	}

	// Display and log the device list
	const string devices = ListDevices(GetDevices());
	cout << devices << endl;
	LogDevices(devices);

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
	int fd;

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

	try {
		while (true) {
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

			close(fd);
			fd = -1;
		}
	}
	catch(const ioexception& e) {
		LOGERROR("%s", e.getmsg().c_str());

		// Fall through
	}

    if (fd >= 0) {
		close(fd);
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

	int i;
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

	// Output the Banner
	Banner(argc, argv);

	int ret = 0;
	int port = 6868;

	InitDisks();

	if (!ParseArgument(argc, argv, port)) {
		ret = EINVAL;
		goto err_exit;
	}

	if (!InitBus()) {
		ret = EPERM;
		goto init_exit;
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
		for (i = 0; i < CtrlMax; i++) {
			if (!ctrl[i] || (data & (1 << i)) == 0) {
				continue;
			}

			// Find the target that has moved to the selection phase
			if (ctrl[i]->Process() == BUS::selection) {
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
			phase = ctrl[actid]->Process();

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

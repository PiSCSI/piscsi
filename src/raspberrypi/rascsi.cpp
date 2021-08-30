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

#include "rascsi.h"

#include "os.h"
#include "filepath.h"
#include "fileio.h"
#include "controllers/scsidev_ctrl.h"
#include "controllers/sasidev_ctrl.h"
#include "devices/device_factory.h"
#include "devices/device.h"
#include "devices/disk.h"
#include "devices/file_support.h"
#include "gpiobus.h"
#include "exceptions.h"
#include "protobuf_util.h"
#include "rascsi_version.h"
#include "rasutil.h"
#include "rascsi_interface.pb.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <spdlog/async.h>
#include <string>
#include <sstream>
#include <iostream>
#include <map>
#include <vector>
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
static volatile bool running;		// Running flag
static volatile bool active;		// Processing flag
vector<SASIDEV *> controllers(CtrlMax);	// Controllers
vector<Device *> devices(CtrlMax * UnitNum);	// Disks
GPIOBUS *bus;						// GPIO Bus
int monsocket;						// Monitor Socket
pthread_t monthread;				// Monitor Thread
pthread_mutex_t ctrl_mutex;					// Semaphore for the ctrl array
static void *MonThread(void *param);
vector<string> log_levels;
string current_log_level;			// Some versions of spdlog do not support get_log_level()
string default_image_folder;
typedef pair<int, int> id_set;
map<string, id_set> files_in_use;
DeviceFactory& device_factory = DeviceFactory::instance();

//---------------------------------------------------------------------------
//
//	Signal Processing
//
//---------------------------------------------------------------------------
void KillHandler(int sig)
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

bool InitService(int port)
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

	running = false;
	active = false;

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
	for (const auto& controller : controllers) {
		if (controller) {
			controller->Reset();
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
		string f = filename[0] == '/' ? filename : default_image_folder + "/" + filename;

		image_file->set_read_only(access(f.c_str(), W_OK));

		struct stat st;
		if (!stat(f.c_str(), &st)) {
			image_file->set_size(st.st_size);
		}
	}
}

const PbDevices GetDevices()
{
	PbDevices pbDevices;

	for (size_t i = 0; i < devices.size(); i++) {
		// skip if unit does not exist or null disk
		Device *device = devices[i];
		if (!device) {
			continue;
		}

		PbDevice *pbDevice = pbDevices.add_devices();

		pbDevice->set_id(i / UnitNum);
		pbDevice->set_unit(i % UnitNum);
		pbDevice->set_vendor(device->GetVendor());
		pbDevice->set_product(device->GetProduct());
		pbDevice->set_revision(device->GetRevision());
		PbDeviceType type = UNDEFINED;
		PbDeviceType_Parse(device->GetType(), &type);
		pbDevice->set_type(type);

		PbDeviceProperties *properties = new PbDeviceProperties();
		pbDevice->set_allocated_properties(properties);
		properties->set_read_only(device->IsReadOnly());
		properties->set_protectable(device->IsProtectable());
		properties->set_removable(device->IsRemovable());
		properties->set_lockable(device->IsLockable());

		PbDeviceStatus *status = new PbDeviceStatus();
		pbDevice->set_allocated_status(status);
		status->set_protected_(device->IsProtected());
		status->set_removed(device->IsRemoved());
		status->set_locked(device->IsLocked());

		const Disk *disk = dynamic_cast<Disk*>(device);
		if (disk) {
			pbDevice->set_block_size(disk->GetSectorSizeInBytes());
		}

		const FileSupport *fileSupport = dynamic_cast<FileSupport *>(device);
		if (fileSupport) {
			Filepath filepath;
			fileSupport->GetPath(filepath);
			PbImageFile *image_file = new PbImageFile();
			GetImageFile(image_file, device->IsRemovable() && !device->IsReady() ? "" : filepath.GetPath());
			pbDevice->set_allocated_file(image_file);
			properties->set_supports_file(true);
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
	for (size_t i = 0; i < controllers.size(); i++) {
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
				if (devices[unitno]->IsSASIHD()) {
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
		if (!sasi_num && !scsi_num) {
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
		LOGERROR("%s", msg.c_str());
	}

	if (fd == -1) {
		if (!msg.empty()) {
			if (status) {
				FPRT(stderr, "Error: ");
				FPRT(stderr, "%s", msg.c_str());
				FPRT(stderr, "\n");
			}
			else {
				FPRT(stdout, "%s", msg.c_str());
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

void LogDevices(const string& devices)
{
	stringstream ss(devices);
	string line;

	while (getline(ss, line, '\n')) {
		LOGINFO("%s", line.c_str());
	}
}

void GetLogLevels(PbServerInfo& serverInfo)
{
	for (const auto& log_level : log_levels) {
		serverInfo.add_log_levels(log_level);
	}
}

void GetDeviceTypeFeatures(PbServerInfo& serverInfo)
{
	PbDeviceTypeProperties *types_properties = serverInfo.add_types_properties();
	types_properties->set_type(SAHD);
	PbDeviceProperties *properties = new PbDeviceProperties();
	types_properties->set_allocated_properties(properties);
	properties->set_supports_file(true);
	properties->set_luns(2);
	for (const auto& block_size : device_factory.GetSasiSectorSizes()) {
		properties->add_block_sizes(block_size);
	}

	types_properties = serverInfo.add_types_properties();
	types_properties->set_type(SCHD);
	properties = new PbDeviceProperties();
	types_properties->set_allocated_properties(properties);
	properties->set_protectable(true);
	properties->set_supports_file(true);
	properties->set_luns(1);
	for (const auto& block_size : device_factory.GetScsiSectorSizes()) {
		properties->add_block_sizes(block_size);
	}

	types_properties = serverInfo.add_types_properties();
	types_properties->set_type(SCRM);
	properties = new PbDeviceProperties();
	types_properties->set_allocated_properties(properties);
	properties->set_protectable(true);
	properties->set_removable(true);
	properties->set_lockable(true);
	properties->set_supports_file(true);
	properties->set_luns(1);
	for (const auto& block_size : device_factory.GetScsiSectorSizes()) {
		properties->add_block_sizes(block_size);
	}

	types_properties = serverInfo.add_types_properties();
	types_properties->set_type(SCMO);
	properties = new PbDeviceProperties();
	types_properties->set_allocated_properties(properties);
	properties->set_protectable(true);
	properties->set_removable(true);
	properties->set_lockable(true);
	properties->set_supports_file(true);
	properties->set_luns(1);
	for (const auto& capacity : device_factory.GetMoCapacities()) {
		properties->add_capacities(capacity);
	}

	types_properties = serverInfo.add_types_properties();
	types_properties->set_type(SCCD);
	properties = new PbDeviceProperties();
	types_properties->set_allocated_properties(properties);
	properties->set_read_only(true);
	properties->set_removable(true);
	properties->set_lockable(true);
	properties->set_supports_file(true);
	properties->set_luns(1);

	types_properties = serverInfo.add_types_properties();
	types_properties->set_type(SCBR);
	properties = new PbDeviceProperties();
	types_properties->set_allocated_properties(properties);
	properties->set_supports_params(true);
	properties->set_luns(1);

	types_properties = serverInfo.add_types_properties();
	types_properties->set_type(SCDP);
	properties = new PbDeviceProperties();
	types_properties->set_allocated_properties(properties);
	properties->set_supports_params(true);
	properties->set_luns(1);
}

void GetAvailableImages(PbServerInfo& serverInfo)
{
	if (!access(default_image_folder.c_str(), F_OK)) {
		for (const auto& entry : filesystem::directory_iterator(default_image_folder)) {
			if (entry.is_regular_file()) {
				GetImageFile(serverInfo.add_image_files(), entry.path().filename());
			}
		}
	}
}

bool SetDefaultImageFolder(const string& f)
{
	string folder = f;

	// If a relative path is specified the path is assumed to be relative to the user's home directory
	if (folder[0] != '/') {
		const passwd *passwd = getpwuid(getuid());
		if (passwd) {
			folder = passwd->pw_dir;
			folder += "/";
			folder += f;
		}
	}

	struct stat info;
	stat(folder.c_str(), &info);
	if (!S_ISDIR(info.st_mode) || access(folder.c_str(), F_OK) == -1) {
		return false;
	}

	default_image_folder = folder;

	LOGINFO("Default image folder set to '%s'", default_image_folder.c_str());

	return true;
}

//---------------------------------------------------------------------------
//
//	Command Processing
//
//---------------------------------------------------------------------------

bool ProcessCmd(int fd, const PbDeviceDefinition& pbDevice, const PbOperation operation, const string& params, bool dryRun)
{
	Filepath filepath;
	Device *device = NULL;
	ostringstream error;

	const int id = pbDevice.id();
	const int unit = pbDevice.unit();
	PbDeviceType type = pbDevice.type();
	bool all = params == "all";

	ostringstream s;
	s << (dryRun ? "Validating: " : "Executing: ");
	s << "operation=" << PbOperation_Name(operation) << ", command params='" << params
			<< "', device id=" << id << ", unit=" << unit << ", type=" << PbDeviceType_Name(type)
			<< ", params='" << pbDevice.params() << "', vendor='" << pbDevice.vendor()
			<< "', product='" << pbDevice.product() << "', revision='" << pbDevice.revision()
			<< "', block size=" << pbDevice.block_size();
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
	Device *map[devices.size()];
	for (size_t i = 0; i < devices.size(); i++) {
		map[i] = devices[i];
	}

	if (operation == ATTACH) {
		if (map[id * UnitNum + unit]) {
			error << "Duplicate ID " << id << ", unit " << unit;
			return ReturnStatus(fd, false, error);
		}

		string filename = pbDevice.params();
		string ext;
		size_t separator = filename.rfind('.');
		if (separator != string::npos) {
			ext = filename.substr(separator + 1);
		}

		// Create a new device, based upon the provided type or filename extension
		device = device_factory.CreateDevice(type, filename, ext);
		if (!device) {
			if (type == UNDEFINED) {
				return ReturnStatus(fd, false, "No device type provided for unknown file extension '" + ext + "'");
			}
			else {
				return ReturnStatus(fd, false, "Unknown device type " + PbDeviceType_Name(type));
			}
		}

		// If no filename was provided the media is considered removed
		if (filename.empty()) {
			device->SetRemoved(true);
		}

		device->SetId(id);
		device->SetLun(unit);
		if (!device->IsReadOnly()) {
			device->SetProtected(pbDevice.protected_());
		}

		try {
			if (!pbDevice.vendor().empty()) {
				device->SetVendor(pbDevice.vendor());
			}
			if (!pbDevice.product().empty()) {
				device->SetProduct(pbDevice.product());
			}
			if (!pbDevice.revision().empty()) {
				device->SetRevision(pbDevice.revision());
			}
		}
		catch(const illegal_argument_exception& e) {
			return ReturnStatus(fd, false, e.getmsg());
		}

		if (pbDevice.block_size()) {
			Disk *disk = dynamic_cast<Disk *>(device);
			if (disk && disk->IsSectorSizeConfigurable()) {
				if (!disk->SetConfiguredSectorSize(pbDevice.block_size())) {
					error << "Invalid block size " << pbDevice.block_size() << " bytes";
					return ReturnStatus(fd, false, error);
				}
			}
			else {
				return ReturnStatus(fd, false, "Block size is not configurable for device type " + PbDeviceType_Name(type));
			}
		}

		FileSupport *fileSupport = dynamic_cast<FileSupport *>(device);

		// File check (type is HD, for removable media drives, CD and MO the medium (=file) may be inserted later)
		if (fileSupport && !device->IsRemovable() && filename.empty()) {
			delete device;

			return ReturnStatus(fd, false, "Device type " + PbDeviceType_Name(type) + " requires a filename");
		}

		// drive checks files
		if (fileSupport && !filename.empty()) {
			filepath.SetPath(filename.c_str());

			try {
				try {
					fileSupport->Open(filepath);
				}
				catch(const file_not_found_exception&) {
					// If the file does not exist search for it in the default image folder
					filepath.SetPath(string(default_image_folder + "/" + filename).c_str());
					fileSupport->Open(filepath);
				}
			}
			catch(const io_exception& e) {
				delete device;

				return ReturnStatus(fd, false, "Tried to open an invalid file '" + string(filepath.GetPath()) + "': " + e.getmsg());
			}

			const string path = filepath.GetPath();
			if (files_in_use.find(path) != files_in_use.end()) {
				delete device;

				const auto& full_id = files_in_use[path];
				error << "Image file '" << filename << "' is already used by ID " << full_id.first << ", unit " << full_id.second;
				return ReturnStatus(fd, false, error);
			}

			files_in_use[path] = make_pair(device->GetId(), device->GetLun());
		}

		// Stop the dry run here, before permanently modifying something
		if (dryRun) {
			return true;
		}

		if (!device->Init(pbDevice.params())) {
			error << "Initialization of " << device->GetType() << " device, ID " << id << ", unit " << unit << " failed";

			delete device;

			return ReturnStatus(fd, false, error);
		}

		// Replace with the newly created unit
		map[id * UnitNum + unit] = device;

		// Re-map the controller
		bool status = MapController(map);
		if (status) {
			LOGINFO("Added new %s device, ID %d, unit %d", device->GetType().c_str(), id, unit);
			return true;
		}

		return ReturnStatus(fd, false, "SASI and SCSI can't be mixed");
	}

	// When detaching all devices no controller/unit tests are required
	if (operation != DETACH || !all) {
		// Does the controller exist?
		if (!dryRun && !controllers[id]) {
			error << "Received a command for non-existing ID " << id;
			return ReturnStatus(fd, false, error);
		}

		// Does the unit exist?
		device = devices[id * UnitNum + unit];
		if (!dryRun && !device) {
			error << "Received a command for ID " << id << ", non-existing unit " << unit;
			return ReturnStatus(fd, false, error);
		}
	}

	FileSupport *fileSupport = dynamic_cast<FileSupport *>(device);

	if (operation == DETACH) {
		if (!all && !params.empty()) {
			return ReturnStatus(fd, false, "Invalid command parameter '" + params + "' for " + PbOperation_Name(DETACH));
		}

		if (!dryRun) {
			// Free the existing unit(s)
			if (all) {
				for (size_t i = 0; i < devices.size(); i++) {
					map[i] = NULL;
				}

				files_in_use.clear();
			}
			else {
				map[id * UnitNum + unit] = NULL;

				if (fileSupport) {
					Filepath filepath;
					fileSupport->GetPath(filepath);
					files_in_use.erase(filepath.GetPath());
				}
			}

			// Re-map the controller, remember the device type because the device gets lost when re-mapping
			const string device_type = device ? device->GetType() : PbDeviceType_Name(UNDEFINED);
			bool status = MapController(map);
			if (status) {
				if (all) {
					LOGINFO("Disconnected all devices");
				}
				else {
					LOGINFO("Disconnected %s device with ID %d, unit %d", device_type.c_str(), id, unit);
				}
				return true;
			}

			return ReturnStatus(fd, false, "SASI and SCSI can't be mixed");
		}
	}

	if ((operation == INSERT || operation == EJECT) && !device->IsRemovable()) {
		return ReturnStatus(fd, false, PbOperation_Name(operation) + " operation denied (" + device->GetType() + " isn't removable)");
	}

	if ((operation == PROTECT || operation == UNPROTECT) && !device->IsProtectable()) {
		return ReturnStatus(fd, false, PbOperation_Name(operation) + " operation denied (" + device->GetType() + " isn't protectable)");
	}

	switch (operation) {
		case INSERT: {
				if (!device->IsRemoved()) {
					return ReturnStatus(fd, false, "Existing medium must first be ejected");
				}

				if (!pbDevice.vendor().empty() || !pbDevice.product().empty() || !pbDevice.revision().empty()) {
					return ReturnStatus(fd, false, "Device name cannot be changed");
				}

				string filename = pbDevice.params();

				if (filename.empty()) {
					return ReturnStatus(fd, false, "Missing filename");
				}

				if (dryRun) {
					return true;
				}

				filepath.SetPath(filename.c_str());

				LOGINFO("Insert file '%s' requested into %s ID %d, unit %d", filename.c_str(), device->GetType().c_str(), id, unit);

				const string path = filepath.GetPath();
				if (files_in_use.find(path) != files_in_use.end()) {
					const auto& full_id = files_in_use[path];
					error << "Image file '" << filename << "' is already used by ID " << full_id.first << ", unit " << full_id.second;
					return ReturnStatus(fd, false, error);
				}

				try {
					try {
						fileSupport->Open(filepath);
					}
					catch(const file_not_found_exception&) {
						// If the file does not exist search for it in the default image folder
						filepath.SetPath(string(default_image_folder + "/" + filename).c_str());
						fileSupport->Open(filepath);
					}
				}
				catch(const io_exception& e) {
					return ReturnStatus(fd, false, "Tried to open an invalid file '" + string(filepath.GetPath()) + "': " + e.getmsg());
				}

				files_in_use[path] = make_pair(device->GetId(), device->GetLun());
			}
			break;

		case EJECT: {
				if (dryRun) {
					return true;
				}

				LOGINFO("Eject requested for %s ID %d, unit %d", device->GetType().c_str(), id, unit);

				// EJECT is idempotent
				if (device->Eject(true) && fileSupport) {
					Filepath filepath;
					fileSupport->GetPath(filepath);
					files_in_use.erase(filepath.GetPath());
				}
			}
			break;

		case PROTECT:
			if (dryRun) {
				return true;
			}

			LOGINFO("Write protection requested for %s ID %d, unit %d", device->GetType().c_str(), id, unit);

			// PROTECT is idempotent
			device->SetProtected(true);
			break;

		case UNPROTECT:
			if (dryRun) {
				return true;
			}

			LOGINFO("Write unprotection requested for %s ID %d, unit %d", device->GetType().c_str(), id, unit);

			// UNPROTECT is idempotent
			device->SetProtected(false);
			break;

		case ATTACH:
		case DETACH:
			assert(dryRun);

			// The non dry-run case was handled above
			return true;

		default:
			return ReturnStatus(fd, false, "Received unknown operation: " + PbOperation_Name(operation));
	}

	return true;
}

bool ProcessCmd(const int fd, const PbCommand& command)
{
	// Dry run first
	const auto files = files_in_use;
	for (int i = 0; i < command.devices().devices_size(); i++) {
		if (!ProcessCmd(fd, command.devices().devices(i), command.operation(), command.params(), true)) {
			return false;
		}
	}

	// Execute
	files_in_use = files;
	for (int i = 0; i < command.devices().devices_size(); i++) {
		if (!ProcessCmd(fd, command.devices().devices(i), command.operation(), command.params(), false)) {
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
	int block_size = 0;
	string name;
	string log_level;

	opterr = 1;
	int opt;
	while ((opt = getopt(argc, argv, "-IiHhG:g:D:d:B:b:N:n:T:t:P:p:f:")) != -1) {
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

			case 'b': {
				block_size = atoi(optarg);
				continue;
			}

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
				if (!SetDefaultImageFolder(optarg)) {
					cerr << "Folder '" << optarg << "' does not exist or is not accessible";
					return false;
				}
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

			default:
				return false;

			case 1:
				// Encountered filename
				break;
		}

		if (optopt) {
			return false;
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
		device->set_block_size(block_size);
		device->set_params(optarg);

		size_t separatorPos = name.find(':');
		if (separatorPos != string::npos) {
			device->set_vendor(name.substr(0, separatorPos));
			name = name.substr(separatorPos + 1);
			separatorPos = name.find(':');
			if (separatorPos != string::npos) {
				device->set_product(name.substr(0, separatorPos));
				device->set_revision(name.substr(separatorPos + 1));
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
		LOGWARN("Invalid log level '%s'", log_level.c_str());
	}

	// Attach all specified devices
	PbCommand command;
	command.set_operation(ATTACH);
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
				throw io_exception("accept() failed");
			}

			// Fetch the command
			PbCommand command;
			DeserializeMessage(fd, command);

			switch(command.operation()) {
				case LOG_LEVEL: {
					bool status = SetLogLevel(command.params());
					if (!status) {
						ReturnStatus(fd, false, "Invalid log level: " + command.params());
					}
					else {
						ReturnStatus(fd);
					}
					break;
				}

				case DEFAULT_FOLDER:
					if (command.params().empty()) {
						ReturnStatus(fd, false, "Can't set default image folder: Missing folder name");
					}

					if (!SetDefaultImageFolder(command.params())) {
						ReturnStatus(fd, false, "Folder '" + command.params() + "' does not exist or is not accessible");
					}
					else {
						ReturnStatus(fd);
					}
					break;

				case SERVER_INFO: {
					PbServerInfo serverInfo;
					serverInfo.set_rascsi_version(rascsi_get_version_string());
					GetLogLevels(serverInfo);
					serverInfo.set_current_log_level(current_log_level);
					serverInfo.set_default_image_folder(default_image_folder);
					GetDeviceTypeFeatures(serverInfo);
					GetAvailableImages(serverInfo);
					serverInfo.set_allocated_devices(new PbDevices(GetDevices()));
					SerializeMessage(fd, serverInfo);
					LogDevices(ListDevices(serverInfo.devices()));
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
		catch(const io_exception& e) {
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
	BUS::phase_t phase;
	// added setvbuf to override stdout buffering, so logs are written immediately and not when the process exits.
	setvbuf(stdout, NULL, _IONBF, 0);
	struct sched_param schparam;

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

	log_levels.push_back("trace");
	log_levels.push_back("debug");
	log_levels.push_back("info");
	log_levels.push_back("warn");
	log_levels.push_back("err");
	log_levels.push_back("critical");
	log_levels.push_back("off");
	SetLogLevel("info");

	// Create a thread-safe stdout logger to process the log messages
	auto logger = stdout_color_mt("rascsi stdout logger");

	// ~/images is the default folder for device image file. For the root user /home/pi/images is the default.
	const int uid = getuid();
	const passwd *passwd = getpwuid(uid);
	if (uid && passwd) {
		string folder = passwd->pw_dir;
		folder += "/images";
		default_image_folder = folder;
	}
	else {
		default_image_folder = "/home/pi/images";
	}

	int port = 6868;

	if (!InitBus()) {
		return EPERM;
	}

	if (!ParseArgument(argc, argv, port)) {
		Cleanup();
		return -1;
	}

	if (!InitService(port)) {
		return EPERM;
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
	running = true;

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
			int now = SysTimer::GetTimerLow();
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
		BYTE data = bus->GetDAT();
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
		active = true;

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
		active = false;
	}

	return 0;
}

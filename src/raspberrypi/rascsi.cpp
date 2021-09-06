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
#include <list>
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
set<int> reserved_ids;
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
	int result = pthread_mutex_init(&ctrl_mutex,NULL);
	if (result != EXIT_SUCCESS){
		LOGERROR("Unable to create a mutex. Err code: %d", result);
		return false;
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
		return false;
	}

	signal(SIGPIPE, SIG_IGN);

	// Bind
	if (bind(monsocket, (struct sockaddr *)&server,
		sizeof(struct sockaddr_in)) < 0) {
		FPRT(stderr, "Error : Already running?\n");
		return false;
	}

	// Create Monitor Thread
	pthread_create(&monthread, NULL, MonThread, NULL);

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

void GetDevice(const Device *device, PbDevice *pb_device)
{
	pb_device->set_id(device->GetId());
	pb_device->set_unit(device->GetLun());
	pb_device->set_vendor(device->GetVendor());
	pb_device->set_product(device->GetProduct());
	pb_device->set_revision(device->GetRevision());
	PbDeviceType type = UNDEFINED;
	PbDeviceType_Parse(device->GetType(), &type);
	pb_device->set_type(type);

	PbDeviceProperties *properties = new PbDeviceProperties();
	pb_device->set_allocated_properties(properties);
	properties->set_read_only(device->IsReadOnly());
	properties->set_protectable(device->IsProtectable());
	properties->set_removable(device->IsRemovable());
	properties->set_lockable(device->IsLockable());

	PbDeviceStatus *status = new PbDeviceStatus();
	pb_device->set_allocated_status(status);
	status->set_protected_(device->IsProtected());
	status->set_removed(device->IsRemoved());
	status->set_locked(device->IsLocked());

	const Disk *disk = dynamic_cast<const Disk*>(device);
	if (disk) {
		pb_device->set_block_size(disk->GetSectorSizeInBytes());
	}

	const FileSupport *file_support = dynamic_cast<const FileSupport *>(device);
	if (file_support) {
		Filepath filepath;
		file_support->GetPath(filepath);
		PbImageFile *image_file = new PbImageFile();
		GetImageFile(image_file, device->IsRemovable() && !device->IsReady() ? "" : filepath.GetPath());
		pb_device->set_allocated_file(image_file);
		properties->set_supports_file(true);
	}
}

void GetDevices(PbServerInfo& serverInfo)
{
	for (size_t i = 0; i < devices.size(); i++) {
		// skip if unit does not exist or null disk
		Device *device = devices[i];
		if (device) {
			PbDevice *pb_device = serverInfo.add_devices();
			GetDevice(device, pb_device);
		}
	}
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

void GetLogLevels(PbServerInfo& server_info)
{
	for (const auto& log_level : log_levels) {
		server_info.add_log_levels(log_level);
	}
}

void GetDeviceTypeFeatures(PbServerInfo& server_info)
{
	PbDeviceTypeProperties *types_properties = server_info.add_types_properties();
	types_properties->set_type(SAHD);
	PbDeviceProperties *properties = new PbDeviceProperties();
	types_properties->set_allocated_properties(properties);
	properties->set_supports_file(true);
	properties->set_luns(2);
	for (const auto& block_size : device_factory.GetSasiSectorSizes()) {
		properties->add_block_sizes(block_size);
	}

	types_properties = server_info.add_types_properties();
	types_properties->set_type(SCHD);
	properties = new PbDeviceProperties();
	types_properties->set_allocated_properties(properties);
	properties->set_protectable(true);
	properties->set_supports_file(true);
	properties->set_luns(1);
	for (const auto& block_size : device_factory.GetScsiSectorSizes()) {
		properties->add_block_sizes(block_size);
	}

	types_properties = server_info.add_types_properties();
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

	types_properties = server_info.add_types_properties();
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

	types_properties = server_info.add_types_properties();
	types_properties->set_type(SCCD);
	properties = new PbDeviceProperties();
	types_properties->set_allocated_properties(properties);
	properties->set_read_only(true);
	properties->set_removable(true);
	properties->set_lockable(true);
	properties->set_supports_file(true);
	properties->set_luns(1);

	types_properties = server_info.add_types_properties();
	types_properties->set_type(SCBR);
	properties = new PbDeviceProperties();
	types_properties->set_allocated_properties(properties);
	properties->set_supports_params(true);
	properties->set_luns(1);

	types_properties = server_info.add_types_properties();
	types_properties->set_type(SCDP);
	properties = new PbDeviceProperties();
	types_properties->set_allocated_properties(properties);
	properties->set_supports_params(true);
	properties->set_luns(1);
}

void GetAvailableImages(PbServerInfo& server_info)
{
	if (!access(default_image_folder.c_str(), F_OK)) {
		for (const auto& entry : filesystem::directory_iterator(default_image_folder)) {
			if (entry.is_regular_file()) {
				GetImageFile(server_info.add_image_files(), entry.path().filename());
			}
		}
	}
}

void GetDeviceInfo(const PbCommand& command, PbResult& result)
{
	set<id_set> id_sets;
	if (command.devices_size() == 0) {
		for (const Device *device : devices) {
			if (device) {
				id_sets.insert(make_pair(device->GetId(), device->GetLun()));
			}
		}
	}
	else {
		for (const auto& device : command.devices()) {
			if (devices[device.id() * UnitNum + device.unit()]) {
				id_sets.insert(make_pair(device.id(), device.unit()));
			}
			else {
				ostringstream error;
				error << "No device for ID " << device.id() << ", unit " << device.unit();
				result.set_status(false);
				result.set_msg(error.str());
				return;
			}
		}
	}

	PbDevices *pb_devices = new PbDevices();

	for (const auto& id_set : id_sets) {
		Device *device = devices[id_set.first * UnitNum + id_set.second];
		PbDevice *pb_device = pb_devices->add_devices();
		GetDevice(device, pb_device);
	}

	result.set_allocated_device_info(pb_devices);
}

void GetServerInfo(PbResult& result)
{
	PbServerInfo *server_info = new PbServerInfo();

	server_info->set_major_version(rascsi_major_version);
	server_info->set_minor_version(rascsi_minor_version);
	server_info->set_patch_version(rascsi_patch_version);
	GetLogLevels(*server_info);
	server_info->set_current_log_level(current_log_level);
	server_info->set_default_image_folder(default_image_folder);
	GetDeviceTypeFeatures(*server_info);
	GetAvailableImages(*server_info);
	GetDevices(*server_info);
	for (int id : reserved_ids) {
		server_info->add_reserved_ids(id);
	}

	result.set_allocated_server_info(server_info);
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

string SetReservedIds(const list<string>& ids_to_reserve)
{
    set<int> reserved;
    for (string id_to_reserve : ids_to_reserve) {
    	int id;
 		if (!GetAsInt(id_to_reserve, id)) {
 			return id_to_reserve;
 		}

 		reserved.insert(id);
    }

    reserved_ids = reserved;


	list<int> ids = { reserved_ids.begin(), reserved_ids.end() };
	ids.sort([](const auto& a, const auto& b) { return a < b; });
	ostringstream s;
	bool isFirst = true;
	for (auto const& id : ids) {
		if (!isFirst) {
			s << ", ";
		}
		s << id;
		isFirst = false;
	}

	LOGINFO("Reserved IDs set to: %s", s.str().c_str());

	return "";
}

void DetachAll()
{
	Device *map[devices.size()];
	for (size_t i = 0; i < devices.size(); i++) {
		map[i] = NULL;
	}

	if (MapController(map)) {
		LOGINFO("Detached all devices");
	}

	FileSupport::UnreserveAll();
}

bool Attach(int fd, const PbDeviceDefinition& pb_device, Device *map[], bool dryRun)
{
	const int id = pb_device.id();
	const int unit = pb_device.unit();
	PbDeviceType type = pb_device.type();
	ostringstream error;

	if (map[id * UnitNum + unit]) {
		error << "Duplicate ID " << id << ", unit " << unit;
		return ReturnStatus(fd, false, error);
	}

	string filename = pb_device.params_size() > 0 ? pb_device.params().Get(0) : "";
	string ext;
	size_t separator = filename.rfind('.');
	if (separator != string::npos) {
		ext = filename.substr(separator + 1);
	}

	// Create a new device, based upon the provided type or filename extension
	Device *device = device_factory.CreateDevice(type, filename, ext);
	if (!device) {
		if (type == UNDEFINED) {
			return ReturnStatus(fd, false, "No device type provided for unknown file extension '" + ext + "'");
		}
		else {
			return ReturnStatus(fd, false, "Unknown device type " + PbDeviceType_Name(type));
		}
	}

	// If no filename was provided the medium is considered removed
	FileSupport *file_support = dynamic_cast<FileSupport *>(device);
	if (file_support) {
		device->SetRemoved(filename.empty());
	}
	else {
		device->SetRemoved(false);
	}

	device->SetId(id);
	device->SetLun(unit);
	if (!device->IsReadOnly()) {
		device->SetProtected(pb_device.protected_());
	}

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
	catch(const illegal_argument_exception& e) {
		return ReturnStatus(fd, false, e.getmsg());
	}

	if (pb_device.block_size()) {
		Disk *disk = dynamic_cast<Disk *>(device);
		if (disk && disk->IsSectorSizeConfigurable()) {
			if (!disk->SetConfiguredSectorSize(pb_device.block_size())) {
				error << "Invalid block size " << pb_device.block_size() << " bytes";
				return ReturnStatus(fd, false, error);
			}
		}
		else {
			return ReturnStatus(fd, false, "Block size is not configurable for device type " + PbDeviceType_Name(type));
		}
	}

	// File check (type is HD, for removable media drives, CD and MO the medium (=file) may be inserted later)
	if (file_support && !device->IsRemovable() && filename.empty()) {
		delete device;

		return ReturnStatus(fd, false, "Device type " + PbDeviceType_Name(type) + " requires a filename");
	}

	// drive checks files
	Filepath filepath;
	if (file_support && !filename.empty()) {
		filepath.SetPath(filename.c_str());

		try {
			try {
				file_support->Open(filepath);
			}
			catch(const file_not_found_exception&) {
				// If the file does not exist search for it in the default image folder
				filepath.SetPath(string(default_image_folder + "/" + filename).c_str());
				file_support->Open(filepath);
			}
		}
		catch(const io_exception& e) {
			delete device;

			return ReturnStatus(fd, false, "Tried to open an invalid file '" + string(filepath.GetPath()) + "': " + e.getmsg());
		}

		int id;
		int unit;
		if (file_support->GetIdsForReservedFile(filepath, id, unit)) {
			delete device;

			error << "Image file '" << filename << "' is already used by ID " << id << ", unit " << unit;
			return ReturnStatus(fd, false, error);
		}

		file_support->ReserveFile(filepath, device->GetId(), device->GetLun());
	}

	// Stop the dry run here, before permanently modifying something
	if (dryRun) {
		delete device;

		return true;
	}

	if (!device->Init(pb_device.params_size() > 0 ? pb_device.params().Get(0) : "")) {
		error << "Initialization of " << device->GetType() << " device, ID " << id << ", unit " << unit << " failed";

		delete device;

		return ReturnStatus(fd, false, error);
	}

	// Replace with the newly created unit
	map[id * UnitNum + unit] = device;

	// Re-map the controller
	if (MapController(map)) {
		ostringstream msg;
		msg << "Attached ";
		if (device->IsReadOnly()) {
			msg << "read-only ";
		}
		else if (device->IsProtectable()) {
			msg << (device->IsProtected() ? "protected " : "unprotected ");
		}
		msg << device->GetType() << " device, ID " << id << ", unit " << unit;
		LOGINFO("%s", msg.str().c_str());

		return true;
	}

	return ReturnStatus(fd, false, "SASI and SCSI can't be mixed");
}

bool Detach(int fd, const PbDeviceDefinition& pb_device, Device *map[], bool dryRun)
{
	if (!dryRun) {
		const int id = pb_device.id();
		const int unit = pb_device.unit();

		Device *device = map[id * UnitNum + unit];

		map[id * UnitNum + unit] = NULL;

		FileSupport *file_support = dynamic_cast<FileSupport *>(device);
		if (file_support) {
			file_support->UnreserveFile();
		}

		// Re-map the controller, remember the device type because the device gets lost when re-mapping
		const string device_type = device ? device->GetType() : PbDeviceType_Name(UNDEFINED);
		bool status = MapController(map);
		if (status) {
			LOGINFO("Detached %s device with ID %d, unit %d", device_type.c_str(), id, unit);
			return true;
		}

		return ReturnStatus(fd, false, "SASI and SCSI can't be mixed");
	}

	return true;
}

bool Insert(int fd, const PbDeviceDefinition& pb_device, Device *device, bool dryRun)
{
	if (!device->IsRemoved()) {
		return ReturnStatus(fd, false, "Existing medium must first be ejected");
	}

	if (!pb_device.vendor().empty() || !pb_device.product().empty() || !pb_device.revision().empty()) {
		return ReturnStatus(fd, false, "Device name cannot be changed");
	}

	string filename = pb_device.params_size() > 0 ? pb_device.params().Get(0): "";
	if (filename.empty()) {
		return ReturnStatus(fd, false, "Missing filename for " + PbOperation_Name(INSERT));
	}

	if (dryRun) {
		return true;
	}

	LOGINFO("Insert file '%s' requested into %s ID %d, unit %d", filename.c_str(), device->GetType().c_str(),
			pb_device.id(), pb_device.unit());

	int id;
	int unit;
	Filepath filepath;
	filepath.SetPath(filename.c_str());
	FileSupport *file_support = dynamic_cast<FileSupport *>(device);
	if (file_support->GetIdsForReservedFile(filepath, id, unit)) {
		ostringstream error;
		error << "Image file '" << filename << "' is already used by ID " << id << ", unit " << unit;
		return ReturnStatus(fd, false, error);
	}

	try {
		try {
			file_support->Open(filepath);
		}
		catch(const file_not_found_exception&) {
			// If the file does not exist search for it in the default image folder
			filepath.SetPath(string(default_image_folder + "/" + filename).c_str());
			file_support->Open(filepath);
		}
	}
	catch(const io_exception& e) {
		return ReturnStatus(fd, false, "Tried to open an invalid file '" + string(filepath.GetPath()) + "': " + e.getmsg());
	}

	file_support->ReserveFile(filepath, device->GetId(), device->GetLun());

	return true;
}

//---------------------------------------------------------------------------
//
//	Command Processing
//
//---------------------------------------------------------------------------

bool ProcessCmd(int fd, const PbDeviceDefinition& pb_device, const PbOperation operation, const vector<string>& params, bool dryRun)
{
	ostringstream error;

	const int id = pb_device.id();
	const int unit = pb_device.unit();
	PbDeviceType type = pb_device.type();

	ostringstream s;
	s << (dryRun ? "Validating: " : "Executing: ");
	s << "operation=" << PbOperation_Name(operation);

	if (!params.empty()) {
		s << ", command params=";
		for (size_t i = 0; i < params.size(); i++) {
			if (i) {
				s << ", ";
			}
			s << "'" << params[i] << "'";
		}
	}

	s << ", device id=" << id << ", unit=" << unit << ", type=" << PbDeviceType_Name(type);

	if (pb_device.params_size()) {
		s << ", device params=";
		for (int i = 0; i < pb_device.params_size(); i++) {
			if (i) {
				s << ", ";
			}
			s << "'" << pb_device.params().Get(i) << "'";
		}
	}

	s << ", vendor='" << pb_device.vendor() << "', product='" << pb_device.product()
			<< "', revision='" << pb_device.revision()
			<< "', block size=" << pb_device.block_size();
	LOGINFO("%s", s.str().c_str());

	// Check the Controller Number
	if (id < 0 || id >= CtrlMax) {
		error << "Invalid ID " << id << " (0-" << CtrlMax - 1 << ")";
		return ReturnStatus(fd, false, error);
	}

	if (reserved_ids.find(id) != reserved_ids.end()) {
		error << "Device ID " << id << " is reserved";
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
		return Attach(fd, pb_device, map, dryRun);
	}

	// Does the controller exist?
	if (!dryRun && !controllers[id]) {
		error << "Received a command for non-existing ID " << id;
		return ReturnStatus(fd, false, error);
	}

	// Does the unit exist?
	Device *device = devices[id * UnitNum + unit];
	if (!device) {
		error << "Received a command for a non-existing device, ID " << id << ", unit " << unit;
		return ReturnStatus(fd, false, error);
	}

	if (operation == DETACH) {
		return Detach(fd, pb_device, map, dryRun);
	}

	if ((operation == INSERT || operation == EJECT) && !device->IsRemovable()) {
		return ReturnStatus(fd, false, PbOperation_Name(operation) + " operation denied (" + device->GetType() + " isn't removable)");
	}

	if ((operation == PROTECT || operation == UNPROTECT) && !device->IsProtectable()) {
		return ReturnStatus(fd, false, PbOperation_Name(operation) + " operation denied (" + device->GetType() + " isn't protectable)");
	}
	if ((operation == PROTECT || operation == UNPROTECT) && !device->IsReady()) {
		return ReturnStatus(fd, false, PbOperation_Name(operation) + " operation denied (" + device->GetType() + " isn't ready)");
	}

	switch (operation) {
		case INSERT:
			return Insert(fd, pb_device, device, dryRun);

		case EJECT:
			if (!dryRun) {
				LOGINFO("Eject requested for %s ID %d, unit %d", device->GetType().c_str(), id, unit);

				// EJECT is idempotent
				device->Eject(true);
			}
			break;

		case PROTECT:
			if (!dryRun) {
				LOGINFO("Write protection requested for %s ID %d, unit %d", device->GetType().c_str(), id, unit);

				// PROTECT is idempotent
				device->SetProtected(true);
			}
			break;

		case UNPROTECT:
			if (!dryRun) {
				LOGINFO("Write unprotection requested for %s ID %d, unit %d", device->GetType().c_str(), id, unit);

				// UNPROTECT is idempotent
				device->SetProtected(false);
			}
			break;

		case ATTACH:
		case DETACH:
			// The non dry-run case has been handled before the switch
			assert(dryRun);
			break;

		default:
			return ReturnStatus(fd, false, "Unknown operation");
	}

	return true;
}

bool ProcessCmd(const int fd, const PbCommand& command)
{
	if (command.operation() == DETACH_ALL) {
		DetachAll();
		return ReturnStatus(fd);
	}
	else if (command.operation() == RESERVE) {
		const list<string> ids = { command.params().begin(), command.params().end() };
		string invalid_id = SetReservedIds(ids);
		if (!invalid_id.empty()) {
			return ReturnStatus(fd, false,"Invalid ID " + invalid_id + " for " + PbOperation_Name(RESERVE));
		}

		return ReturnStatus(fd);
	}

	const vector<string> params = { command.params().begin(), command.params().end() };

	// Remember the list of reserved files, than run the dry run
	const auto reserved_files = FileSupport::GetReservedFiles();
	for (int i = 0; i < command.devices_size(); i++) {
		if (!ProcessCmd(fd, command.devices(i), command.operation(), params, true)) {
			// Dry run failed, restore the file list
			FileSupport::SetReservedFiles(reserved_files);
			return false;
		}
	}

	// Restore list of reserved files, then execute the command
	FileSupport::SetReservedFiles(reserved_files);
	for (int i = 0; i < command.devices_size(); i++) {
		if (!ProcessCmd(fd, command.devices(i), command.operation(), params, false)) {
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
	PbCommand command;
	int id = -1;
	bool is_sasi = false;
	int max_id = 7;
	PbDeviceType type = UNDEFINED;
	int block_size = 0;
	string name;
	string log_level;

	opterr = 1;
	int opt;
	while ((opt = getopt(argc, argv, "-IiHhG:g:D:d:B:b:N:n:T:t:P:p:R:r:F:f:")) != -1) {
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
				if (!GetAsInt(optarg, block_size)) {
					cerr << "Invalid block size " << optarg << endl;
					return false;
				}
				continue;
			}

			case 'd': {
				char* end;
				id = strtol(optarg, &end, 10);
				if (*end || id < 0 || max_id < id) {
					cerr << optarg << ": invalid " << (is_sasi ? "HD" : "ID") << " (0-" << max_id << ")" << endl;
					return false;
				}
				continue;
			}

			case 'f':
				if (!SetDefaultImageFolder(optarg)) {
					cerr << "Folder '" << optarg << "' does not exist or is not accessible";
					return false;
				}
				continue;

			case 'g':
				log_level = optarg;
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

			case 'r': {
					stringstream ss(optarg);
					string id;

					list<string> ids;
					while (getline(ss, id, ',')) {
						ids.push_back(id);
					}

					string invalid_id = SetReservedIds(ids);
					if (!invalid_id.empty()) {
						cerr << "Invalid ID " << invalid_id << " for " << PbOperation_Name(RESERVE);
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
		PbDeviceDefinition *device = command.add_devices();
		device->set_id(id);
		device->set_unit(unit);
		device->set_type(type);
		device->set_block_size(block_size);
		device->add_params(optarg);

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
	command.set_operation(ATTACH);

	if (!ProcessCmd(-1, command)) {
		return false;
	}

	// Display and log the device list
	PbServerInfo server_info;
	GetDevices(server_info);
	const list<PbDevice>& devices = { server_info.devices().begin(), server_info.devices().end() };
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
					LOGTRACE(string("Received " + PbOperation_Name(LOG_LEVEL) + " command").c_str());

					string log_level = command.params_size() > 0 ? command.params().Get(0) : "";
					bool status = SetLogLevel(log_level);
					if (!status) {
						ReturnStatus(fd, false, "Invalid log level: " + log_level);
					}
					else {
						ReturnStatus(fd);
					}
					break;
				}

				case DEFAULT_FOLDER: {
					LOGTRACE(string("Received " + PbOperation_Name(DEFAULT_FOLDER) + " command").c_str());

					string folder = command.params_size() > 0 ? command.params().Get(0) : "";
					if (folder.empty()) {
						ReturnStatus(fd, false, "Can't set default image folder: Missing folder name");
					}

					if (!SetDefaultImageFolder(folder)) {
						ReturnStatus(fd, false, "Folder '" + folder + "' does not exist or is not accessible");
					}
					else {
						ReturnStatus(fd);
					}
					break;
				}

				case DEVICE_INFO: {
					LOGTRACE(string("Received " + PbOperation_Name(DEVICE_INFO) + " command").c_str());

					PbResult result;
					result.set_status(true);
					GetDeviceInfo(command, result);
					SerializeMessage(fd, result);
					const list<PbDevice>& devices ={ result.device_info().devices().begin(), result.device_info().devices().end() };

					// For backwards compatibility: Log device list if information on all devices was requested.
					if (command.devices_size() == 0) {
						LogDevices(ListDevices(devices));
					}
					break;
				}

				case SERVER_INFO: {
					LOGTRACE(string("Received " + PbOperation_Name(SERVER_INFO) + " command").c_str());

					PbResult result;
					result.set_status(true);
					GetServerInfo(result);
					SerializeMessage(fd, result);
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

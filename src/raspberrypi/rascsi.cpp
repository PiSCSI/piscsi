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
#include "rasctl_interface.pb.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <spdlog/async.h>
#include <string>
#include <sstream>
#include <iostream>

using namespace std;
using namespace spdlog;
using namespace rasctl_interface;

//---------------------------------------------------------------------------
//
//  Constant declarations
//
//---------------------------------------------------------------------------
#define CtrlMax	8					// Maximum number of SCSI controllers
#define UnitNum	2					// Number of units around controller
#ifdef BAREMETAL
#define FPRT(fp, ...) printf( __VA_ARGS__ )
#else
#define FPRT(fp, ...) fprintf(fp, __VA_ARGS__ )
#endif	// BAREMETAL

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
#ifdef BAREMETAL
FATFS fatfs;						// FatFS
#else
int monsocket;						// Monitor Socket
pthread_t monthread;				// Monitor Thread
pthread_mutex_t ctrl_mutex;					// Semaphore for the ctrl array
static void *MonThread(void *param);
#endif	// BAREMETAL

#ifndef BAREMETAL
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
#endif	// BAREMETAL

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
		FPRT(stdout,"  hdf : SASI HD image(XM6 SASI HD image)\n");
		FPRT(stdout,"  hds : SCSI HD image(XM6 SCSI HD image)\n");
		FPRT(stdout,"  hdn : SCSI HD image(NEC GENUINE)\n");
		FPRT(stdout,"  hdi : SCSI HD image(Anex86 HD image)\n");
		FPRT(stdout,"  nhd : SCSI HD image(T98Next HD image)\n");
		FPRT(stdout,"  hda : SCSI HD image(APPLE GENUINE)\n");
		FPRT(stdout,"  mos : SCSI MO image(XM6 SCSI MO image)\n");
		FPRT(stdout,"  iso : SCSI CD image(ISO 9660 image)\n");

#ifndef BAREMETAL
		exit(0);
#endif	// BAREMETAL
	}
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL Init()
{
#ifndef BAREMETAL
	struct sockaddr_in server;
	int yes, result;

	result = pthread_mutex_init(&ctrl_mutex,NULL);
	if(result != EXIT_SUCCESS){
		LOGERROR("Unable to create a mutex. Err code: %d",result);
		return FALSE;
	}

	// Create socket for monitor
	monsocket = socket(PF_INET, SOCK_STREAM, 0);
	memset(&server, 0, sizeof(server));
	server.sin_family = PF_INET;
	server.sin_port   = htons(6868);
	server.sin_addr.s_addr = htonl(INADDR_ANY);

	// allow address reuse
	yes = 1;
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
#endif // BAREMETAL

	// GPIOBUS creation
	bus = new GPIOBUS();

	// GPIO Initialization
	if (!bus->Init()) {
		return FALSE;
	}

	// Bus Reset
	bus->Reset();

	// Controller initialization
	for (int i = 0; i < CtrlMax; i++) {
		ctrl[i] = NULL;
	}

	// Disk Initialization
	for (int i = 0; i < CtrlMax; i++) {
		disk[i] = NULL;
	}

	// Other
	running = FALSE;
	active = FALSE;

	return TRUE;
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
	bus->Cleanup();

	// Discard the GPIOBUS object
	delete bus;

#ifndef BAREMETAL
	// Close the monitor socket
	if (monsocket >= 0) {
		close(monsocket);
	}

	pthread_mutex_destroy(&ctrl_mutex);
#endif // BAREMETAL
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
//	List Devices
//
//---------------------------------------------------------------------------
void ListDevice(FILE *fp)
{
	Filepath filepath;
	char type[5];
	char dev_status[_MAX_FNAME+26];

	bool find = false;
	type[4] = 0;

	for (int i = 0; i < CtrlMax * UnitNum; i++) {
		strncpy(dev_status,"",sizeof(dev_status));
		// Initialize ID and unit number
		int id = i / UnitNum;
		int un = i % UnitNum;
		Disk *pUnit = disk[i];

		// skip if unit does not exist or null disk
		if (pUnit == NULL || pUnit->IsNULL()) {
			continue;
		}

		// Output the header
        if (!find) {
			FPRT(fp, "\n");
			FPRT(fp, "+----+----+------+-------------------------------------\n");
			LOGINFO( "+----+----+------+-------------------------------------");
			FPRT(fp, "| ID | UN | TYPE | DEVICE STATUS\n");
			LOGINFO( "| ID | UN | TYPE | DEVICE STATUS\n");
			FPRT(fp, "+----+----+------+-------------------------------------\n");
			find = TRUE;
		}

		// ID,UNIT,Type,Device Status
		type[0] = (char)(pUnit->GetID() >> 24);
		type[1] = (char)(pUnit->GetID() >> 16);
		type[2] = (char)(pUnit->GetID() >> 8);
		type[3] = (char)(pUnit->GetID());

		// mount status output
		if (pUnit->GetID() == MAKEID('S', 'C', 'B', 'R')) {
			strncpy(dev_status,"X68000 HOST BRIDGE",sizeof(dev_status));
		} else if (pUnit->GetID() == MAKEID('S', 'C', 'D', 'P')) {
			strncpy(dev_status,"DaynaPort SCSI/Link",sizeof(dev_status));
		} else {
			pUnit->GetPath(filepath);
			snprintf(dev_status, sizeof(dev_status), "%s", 
				(pUnit->IsRemovable() && !pUnit->IsReady()) ?
				"NO MEDIA" : filepath.GetPath());
		}

		// Write protection status
		if (pUnit->IsRemovable() && pUnit->IsReady() && pUnit->IsWriteP()) {
			strcat(dev_status, " (WRITEPROTECT)");
		}
		FPRT(fp, "|  %d |  %d | %s | %s\n", id, un, type, dev_status);
		LOGINFO( "|  %d |  %d | %s | %s", id, un, type, dev_status);

	}

	// If there is no controller, find will be null
	if (!find) {
		FPRT(fp, "No images currently attached.\n");
		return;
	}

	FPRT(fp, "+----+----+------+-------------------------------------\n");
	LOGINFO( "+----+----+------+-------------------------------------");
}

//---------------------------------------------------------------------------
//
//	Controller Mapping
//
//---------------------------------------------------------------------------
void MapController(FILE *fp, Disk **map)
{
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
			FPRT(fp, "Error : SASI and SCSI can't be mixed\n");
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
}

bool ReturnStatus(FILE *fp, bool status = true, const string msg = "") {
#ifdef BAREMETAL
	if (msg.length()) {
		FPRT(fp, msg.c_str());
		FPRT(fp, "\n");
	}
#else
	Result result;
	result.set_msg(msg + "\n");
	result.set_status(status);

	string data;
	result.SerializeToString(&data);
	SerializeProtobufData(fp, data);
#endif

	return status;
}

void SetLogLevel(const string& log_level) {
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
		LOGWARN("Invalid log level '%s', falling back to 'trace'", log_level.c_str());
		set_level(level::trace);
	}
}

//---------------------------------------------------------------------------
//
//	Command Processing
//
//---------------------------------------------------------------------------
bool ProcessCmd(FILE *fp, const Command &command)
{
	Disk *map[CtrlMax * UnitNum];
	Filepath filepath;
	Disk *pUnit;

	int id = command.id();
	int un = command.un();
	Operation cmd = command.cmd();
	DeviceType type = command.type();
	string params = command.has_params() ? command.params().c_str() : "";

	ostringstream s;
	s << "Processing: cmd=" << Operation_Name(cmd) << ", id=" << id << ", un=" << un
			<< ", type=" << DeviceType_Name(type) << ", params=" << params << endl;
	LOGINFO(s.str().c_str());

	// Copy the Unit List
	memcpy(map, disk, sizeof(disk));

	// Check the Controller Number
	if (id < 0 || id >= CtrlMax) {
		return ReturnStatus(fp, false, "Error : Invalid ID");
	}

	// Check the Unit Number
	if (un < 0 || un >= UnitNum) {
		return ReturnStatus(fp, false, "Error : Invalid unit number");
	}

	// Connect Command
	if (cmd == ATTACH) {
		string ext;

		// Distinguish between SASI and SCSI
		if (type == SASI_HD) {
			// Check the extension
			int len = params.length();
			if (len < 5 || params[len - 4] != '.') {
				return ReturnStatus(fp, false);
			}

			// If the extension is not SASI type, replace with SCSI
			ext = params.substr(len - 3);
			if (ext != "hdf") {
				type = SCSI_HD;
			}
		}

		// Create a new drive, based upon type
		pUnit = NULL;
		switch (type) {
			case SASI_HD:		// HDF
				pUnit = new SASIHD();
				break;
			case SCSI_HD:		// HDS/HDN/HDI/NHD/HDA
				if (ext == "hdn" || ext == "hdi" || ext == "nhd") {
					pUnit = new SCSIHD_NEC();
				} else if (ext == "hda") {
					pUnit = new SCSIHD_APPLE();
				} else {
					pUnit = new SCSIHD();
				}
				break;
			case MO:
				pUnit = new SCSIMO();
				break;
			case CD:
				pUnit = new SCSICD();
				break;
			case BR:
				pUnit = new SCSIBR();
				break;
			// case NUVOLINK:
			// 	pUnit = new SCSINuvolink();
			// 	break;
			case DAYNAPORT:
				pUnit = new SCSIDaynaPort();
				break;
			default:
				ostringstream msg;
				msg << "rasctl sent a command for an invalid drive type: " << type;
				return ReturnStatus(fp, false, msg.str().c_str());
		}

		// drive checks files
		if (type != BR && type != DAYNAPORT && command.has_params()) {
			// Strip the image file extension from device file names, so that device files can be used as drive images
			string file = params.find("/dev/") ? params : params.substr(0, params.length() - 4);

			// Set the Path
			filepath.SetPath(file.c_str());

			// Open the file path
			if (!pUnit->Open(filepath)) {
				delete pUnit;

				LOGWARN("rasctl tried to open an invalid file %s", file.c_str());

				ostringstream msg;
				msg << "Error : File open error [" << file << "]";
				return ReturnStatus(fp, false, msg.str().c_str());
			}
		}

		// Set the cache to write-through
		pUnit->SetCacheWB(FALSE);

		// Replace with the newly created unit
		map[id * UnitNum + un] = pUnit;

		// Re-map the controller
		MapController(fp, map);

		LOGINFO("rasctl added new %s device. ID: %d UN: %d", DeviceType_Name(type).c_str(), id, un);

		return ReturnStatus(fp, true);
	}

	// Does the controller exist?
	if (ctrl[id] == NULL) {
		LOGWARN("rasctl sent a command for invalid controller %d", id);

		return ReturnStatus(fp, false, "Error : No such device");
	}

	// Does the unit exist?
	pUnit = disk[id * UnitNum + un];
	if (pUnit == NULL) {
		LOGWARN("rasctl sent a command for invalid unit ID %d UN %d", id, un);

		return ReturnStatus(fp, false, "Error : No such device");
	}

	// Disconnect Command
	if (cmd == DETACH) {
		LOGINFO("rasctl command disconnect %s at ID: %d UN: %d", DeviceType_Name(type).c_str(), id, un);

		// Free the existing unit
		map[id * UnitNum + un] = NULL;

		// Re-map the controller
		MapController(fp, map);
		return ReturnStatus(fp, true);
	}

	// Valid only for MO or CD
	if (pUnit->GetID() != MAKEID('S', 'C', 'M', 'O') &&
		pUnit->GetID() != MAKEID('S', 'C', 'C', 'D')) {
		LOGWARN("rasctl sent an Insert/Eject/Protect command (%d) for incompatible type %s", cmd, DeviceType_Name(type).c_str());

		ostringstream msg;
		msg << "Error : Operation denied (Device type " << DeviceType_Name(type).c_str() << " isn't removable)";
		return ReturnStatus(fp, false, msg.str().c_str());
	}

	switch (cmd) {
		case INSERT:
			// Set the file path
			filepath.SetPath(params.c_str());
			LOGINFO("rasctl commanded insert file %s into %s ID: %d UN: %d", params.c_str(), DeviceType_Name(type).c_str(), id, un);

			// Open the file
			if (!pUnit->Open(filepath)) {
				ostringstream msg;
				msg << "Error : File open error [" << params << "]";

				return ReturnStatus(fp, false, msg.str().c_str());
			}
			break;

		case EJECT:
			LOGINFO("rasctl commands eject %s ID: %d UN: %d", DeviceType_Name(type).c_str(), id, un);
			pUnit->Eject(TRUE);
			break;

		case PROTECT:
			if (pUnit->GetID() != MAKEID('S', 'C', 'M', 'O')) {
				LOGWARN("rasctl sent an invalid PROTECT command for %s ID: %d UN: %d", DeviceType_Name(type).c_str(), id, un);

				return ReturnStatus(fp, false, "Error : Operation denied (Device isn't MO)");
			}
			LOGINFO("rasctl is setting write protect to %d for %s ID: %d UN: %d",!pUnit->IsWriteP(), DeviceType_Name(type).c_str(), id, un);
			pUnit->WriteP(!pUnit->IsWriteP());
			break;
		default:
			ostringstream msg;
			msg << "Received unknown command from rasctl: " << cmd;
			LOGWARN(msg.str().c_str());
			return ReturnStatus(fp, false, msg.str().c_str());
	}

	return ReturnStatus(fp, true);
}

bool has_suffix(const string& filename, const string& suffix) {
    return filename.size() >= suffix.size() && !filename.compare(filename.size() - suffix.size(), suffix.size(), suffix);
}

//---------------------------------------------------------------------------
//
//	Argument Parsing
//
//---------------------------------------------------------------------------
#ifdef BAREMETAL
BOOL ParseConfig(int argc, char* argv[])
{
	FIL fp;
	char line[512];
	int id;
	int un;
	DeviceType type;
	char *argID;
	char *argPath;
	int len;
	char *ext;

	// Mount the SD card
	FRESULT fr = f_mount(&fatfs, "", 1);
	if (fr != FR_OK) {
		FPRT(stderr, "Error : SD card mount failed.\n");
		return FALSE;
	}

	// If there is no setting file, the processing is interrupted
	fr = f_open(&fp, "rascsi.ini", FA_READ);
	if (fr != FR_OK) {
		return FALSE;
	}

	// Start Decoding

	while (TRUE) {
		// Get one Line
		memset(line, 0x00, sizeof(line));
		if (f_gets(line, sizeof(line) -1, &fp) == NULL) {
			break;
		}

		// Delete the CR/LF
		len = strlen(line);
		while (len > 0) {
			if (line[len - 1] != '\r' && line[len - 1] != '\n') {
				break;
			}
			line[len - 1] = '\0';
			len--;
		}

		// Get the ID and Path
		argID = &line[0];
		argPath = &line[4];
		line[3] = '\0';

		// Check if the line is an empty string
		if (argID[0] == '\0' || argPath[0] == '\0') {
			continue;
		}

		if (strlen(argID) == 3 && xstrncasecmp(argID, "id", 2) == 0) {
			// ID or ID Format

			// Check that the ID number is valid (0-7)
			if (argID[2] < '0' || argID[2] > '7') {
				FPRT(stderr,
					"Error : Invalid argument(IDn n=0-7) [%c]\n", argID[2]);
				goto parse_error;
			}

			// The ID unit is good
            id = argID[2] - '0';
			un = 0;
		} else if (xstrncasecmp(argID, "hd", 2) == 0) {
			// HD or HD format

			if (strlen(argID) == 3) {
				// Check that the HD number is valid (0-9)
				if (argID[2] < '0' || argID[2] > '9') {
					FPRT(stderr,
						"Error : Invalid argument(HDn n=0-15) [%c]\n", argID[2]);
					goto parse_error;
				}

				// ID was confirmed
				id = (argID[2] - '0') / UnitNum;
				un = (argID[2] - '0') % UnitNum;
			} else if (strlen(argID) == 4) {
				// Check that the HD number is valid (10-15)
				if (argID[2] != '1' || argID[3] < '0' || argID[3] > '5') {
					FPRT(stderr,
						"Error : Invalid argument(HDn n=0-15) [%c]\n", argID[2]);
					goto parse_error;
				}

				// The ID unit is good - create the id and unit number
				id = ((argID[3] - '0') + 10) / UnitNum;
				un = ((argID[3] - '0') + 10) % UnitNum;
				argPath++;
			} else {
				FPRT(stderr,
					"Error : Invalid argument(IDn or HDn) [%s]\n", argID);
				goto parse_error;
			}
		} else {
			FPRT(stderr,
				"Error : Invalid argument(IDn or HDn) [%s]\n", argID);
			goto parse_error;
		}

		// Skip if there is already an active device
		if (disk[id * UnitNum + un] &&
			!disk[id * UnitNum + un]->IsNULL()) {
			continue;
		}

		// Check ethernet and host bridge
		if (!strcasecmp(argPath, "bridge")) {
			type = BR;
		} else {
			// Check the path length
			len = strlen(argPath);
			if (len < 5) {
				FPRT(stderr,
					"Error : Invalid argument(File path is short) [%s]\n",
					argPath);
				goto parse_error;
			}

			// Does the file have an extension?
			if (argPath[len - 4] != '.') {
				FPRT(stderr,
					"Error : Invalid argument(No extension) [%s]\n", argPath);
				goto parse_error;
			}

			// Figure out what the type is
			ext = &argPath[len - 3];
			if (!strcasecmp(ext, "hdf") || !strcasecmp(ext, "hds") || !strcasecmp(ext, "hdn") ||
				!strcasecmp(ext, "hdi") || !strcasecmp(ext, "nhd") || !strcasecmp(ext, "hda")) {
				type = SASI_HD;
			} else if (!strcasecmp(ext, "mos")) {
				type = MO;
			} else if (!strcasecmp(ext, "iso")) {
				type = CD;
			}
		}

		// Execute the command
		Command command;
		command.set_id(id);
		command.set_un(un);
		command.set_cmd(0);
		command.set_type(type);
		command.set_file(argPath);
		if (!ProcessCmd(stderr, command)) {
			goto parse_error;
		}
	}

	// Close the configuration file
	f_close(&fp);

	// Display the device list
	ListDevice(stdout);

	return TRUE;

parse_error:

	// Close the configuration file
	f_close(&fp);

	return FALSE;
}
#else
bool ParseArgument(int argc, char* argv[])
{
	int id = -1;
	bool is_sasi = false;
	int max_id = 7;
	string log_level = "trace";

	int opt;
	while ((opt = getopt(argc, argv, "-IiHhL:l:D:d:")) != -1) {
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

			case 'l':
				log_level = optarg;
				continue;

			case 'd': {
				char* end;
				id = strtol(optarg, &end, 10);
				if (*end || id < 0 || max_id < id) {
					cerr << optarg << ": invalid " << (is_sasi ? "HD" : "ID") << "(0-" << max_id << ")" << endl;
					return false;
				}
				continue;
			}

			default:
				return false;

			case 1:
				break;
		}

		if (id < 0) {
			cerr << optarg << ": ID not specified" << endl;
			return false;
		} else if (disk[id] && !disk[id]->IsNULL()) {
			cerr << id << ": duplicate ID" << endl;
			return false;
		}

		string path = optarg;
		DeviceType type = SASI_HD;
		if (has_suffix(path, ".hdf")
			|| has_suffix(path, ".hds")
			|| has_suffix(path, ".hdn")
			|| has_suffix(path, ".hdi")
			|| has_suffix(path, ".hda")
			|| has_suffix(path, ".nhd")) {
			type = SASI_HD;
		} else if (has_suffix(path, ".mos")) {
			type = MO;
		} else if (has_suffix(path, ".iso")) {
			type = CD;
		} else if (path == "bridge") {
			type = BR;
		} else if (path == "daynaport") {
			type = DAYNAPORT;
		}

		int un = 0;
		if (is_sasi) {
			un = id % UnitNum;
			id /= UnitNum;
		}

		// Execute the command
		Command command;
		command.set_id(id);
		command.set_un(un);
		command.set_cmd(ATTACH);
		command.set_type(type);
		command.set_params(path);
		if (!ProcessCmd(stderr, command)) {
			return false;
		}
		id = -1;
	}

	SetLogLevel(log_level);

	// Display the device list
	ListDevice(stdout);
	return true;
}
#endif  // BAREMETAL

#ifndef BAREMETAL
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
    FILE *fp;

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
				throw ioexception();
			}

			// Fetch the command
			fp = fdopen(fd, "r+");
			if (!fp) {
				throw ioexception();
			}

			Command command;
			command.ParseFromString(DeserializeProtobufData(fd));

			// List all of the devices
			if (command.cmd() == LIST) {
				ListDevice(fp);
			}
			else if (command.cmd() == LOG_LEVEL) {
				SetLogLevel(command.params());
			}
			else {
				// Wait until we become idle
				while (active) {
					usleep(500 * 1000);
				}

				ProcessCmd(fp, command);
			}

            fclose(fp);
            fp = NULL;
			close(fd);
			fd = -1;
		}
	}
	catch(const ioexception& e) {
		// Fall through
	}

    if (fp) {
    	fclose(fp);
    }
    if (fd >= 0) {
		close(fd);
	}

	return NULL;
}
#endif	// BAREMETAL

//---------------------------------------------------------------------------
//
//	Main processing
//
//---------------------------------------------------------------------------
#ifdef BAREMETAL
extern "C"
int startrascsi(void)
{
	int argc = 0;
	char** argv = NULL;
#else
int main(int argc, char* argv[])
{
#endif	// BAREMETAL
	int i;
	int ret;
	int actid;
	DWORD now;
	BUS::phase_t phase;
	BYTE data;
	// added setvbuf to override stdout buffering, so logs are written immediately and not when the process exits.
	setvbuf(stdout, NULL, _IONBF, 0);
#ifndef BAREMETAL
	struct sched_param schparam;
#endif	// BAREMETAL

	set_level(level::trace);
	// Create a thread-safe stdout logger to process the log messages
	auto logger = stdout_color_mt("rascsi stdout logger");

	// Output the Banner
	Banner(argc, argv);

	// Initialize
	ret = 0;
	if (!Init()) {
		ret = EPERM;
		goto init_exit;
	}

	// Reset
	Reset();

#ifdef BAREMETAL
	// BUSY assert (to hold the host side)
	bus->SetBSY(TRUE);

	// Argument parsing
	if (!ParseConfig(argc, argv)) {
		ret = EINVAL;
		goto err_exit;
	}
#else
	// Argument parsing
	if (!ParseArgument(argc, argv)) {
		ret = EINVAL;
		goto err_exit;
	}
#endif

#ifdef BAREMETAL
	// Release the busy signal
	bus->SetBSY(FALSE);
#endif

#ifndef BAREMETAL
    // Set the affinity to a specific processor core
	FixCpu(3);

#ifdef USE_SEL_EVENT_ENABLE
	// Scheduling policy setting (highest priority)
	schparam.sched_priority = sched_get_priority_max(SCHED_FIFO);
	sched_setscheduler(0, SCHED_FIFO, &schparam);
#endif	// USE_SEL_EVENT_ENABLE
#endif	// BAREMETAL

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
#if !defined(BAREMETAL)
			usleep(0);
#endif	// !BAREMETAL
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

#if !defined(USE_SEL_EVENT_ENABLE) && !defined(BAREMETAL)
		// Scheduling policy setting (highest priority)
		schparam.sched_priority = sched_get_priority_max(SCHED_FIFO);
		sched_setscheduler(0, SCHED_FIFO, &schparam);
#endif	// !USE_SEL_EVENT_ENABLE && !BAREMETAL

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


#if !defined(USE_SEL_EVENT_ENABLE) && !defined(BAREMETAL)
		// Set the scheduling priority back to normal
		schparam.sched_priority = 0;
		sched_setscheduler(0, SCHED_OTHER, &schparam);
#endif	// !USE_SEL_EVENT_ENABLE && !BAREMETAL

		// End the target travel
		active = FALSE;
	}

err_exit:
	// Cleanup
	Cleanup();

init_exit:
#if !defined(BAREMETAL)
	exit(ret);
#else
	return ret;
#endif	// BAREMETAL
}

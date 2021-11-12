//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//	Copyright (C) 2020-2021 Contributors to the RaSCSI project
//	[ Send Control Command ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "rascsi_version.h"
#include "protobuf_util.h"
#include "rasutil.h"
#include "rasctl_commands.h"
#include "rascsi_interface.pb.h"
#include <iostream>
#include <list>

// Separator for the INQUIRY name components
#define COMPONENT_SEPARATOR ':'

using namespace std;
using namespace rascsi_interface;
using namespace ras_util;
using namespace protobuf_util;

PbOperation ParseOperation(const char *optarg)
{
	switch (tolower(optarg[0])) {
		case 'a':
			return ATTACH;

		case 'd':
			return DETACH;

		case 'i':
			return INSERT;

		case 'e':
			return EJECT;

		case 'p':
			return PROTECT;

		case 'u':
			return UNPROTECT;

		case 's':
			return DEVICES_INFO;

		default:
			return NONE;
	}
}

PbDeviceType ParseType(const char *optarg)
{
	string t = optarg;
	transform(t.begin(), t.end(), t.begin(), ::toupper);

	PbDeviceType type;
	if (PbDeviceType_Parse(t, &type)) {
		return type;
	}
	else {
		// Parse convenience device types (shortcuts)
		switch (tolower(optarg[0])) {
			case 'c':
				return SCCD;

			case 'b':
				return SCBR;

			case 'd':
				return SCDP;

			case 'h':
				return SCHD;

			case 'm':
				return SCMO;

			case 'r':
				return SCRM;
		}
	}

	return UNDEFINED;
}

int main(int argc, char* argv[])
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	// Display help
	if (argc < 2) {
		cerr << "SCSI Target Emulator RaSCSI Controller" << endl;
		cerr << "version " << rascsi_get_version_string() << " (" << __DATE__ << ", " << __TIME__ << ")" << endl;
		cerr << "Usage: " << argv[0] << " -i ID [-u UNIT] [-c CMD] [-C FILE] [-t TYPE] [-b BLOCK_SIZE] [-n NAME] [-f FILE|PARAM] ";
		cerr << "[-F IMAGE_FOLDER] [-L LOG_LEVEL] [-h HOST] [-p PORT] [-r RESERVED_IDS] ";
		cerr << "[-C FILENAME:FILESIZE] [-d FILENAME] [-w FILENAME] [-R CURRENT_NAME:NEW_NAME] [-x CURRENT_NAME:NEW_NAME] ";
		cerr << "[-e] [-E FILENAME] [-D] [-I] [-l] [-L] [-m] [-O] [-s] [-v] [-V] [-y] [-X]" << endl;
		cerr << " where  ID := {0-7}" << endl;
		cerr << "        UNIT := {0-31}, default is 0" << endl;
		cerr << "        CMD := {attach|detach|insert|eject|protect|unprotect|show}" << endl;
		cerr << "        TYPE := {sahd|schd|scrm|sccd|scmo|scbr|scdp} or convenience type {hd|rm|mo|cd|bridge|daynaport}" << endl;
		cerr << "        BLOCK_SIZE := {256|512|1024|2048|4096) bytes per hard disk drive block" << endl;
		cerr << "        NAME := name of device to attach (VENDOR:PRODUCT:REVISION)" << endl;
		cerr << "        FILE|PARAM := image file path or device-specific parameter" << endl;
		cerr << "        IMAGE_FOLDER := default location for image files, default is '~/images'" << endl;
		cerr << "        HOST := rascsi host to connect to, default is 'localhost'" << endl;
		cerr << "        PORT := rascsi port to connect to, default is 6868" << endl;
		cerr << "        RESERVED_IDS := comma-separated list of IDs to reserve" << endl;
		cerr << "        LOG_LEVEL := log level {trace|debug|info|warn|err|critical|off}, default is 'info'" << endl;
		cerr << " If CMD is 'attach' or 'insert' the FILE parameter is required." << endl;
		cerr << "Usage: " << argv[0] << " -l" << endl;
		cerr << "       Print device list." << endl;

		exit(EXIT_SUCCESS);
	}

	PbCommand command;
	list<PbDeviceDefinition> devices;
	PbDeviceDefinition* device = command.add_devices();
	device->set_id(-1);
	const char *hostname = "localhost";
	int port = 6868;
	string param;
	string log_level;
	string default_folder;
	string reserved_ids;
	string image_params;
	string filename;
	bool list = false;

	opterr = 1;
	int opt;
	while ((opt = getopt(argc, argv, "elmsvDINOTVXa:b:c:d:f:h:i:n:p:r:t:u:x:C:E:F:L:R:")) != -1) {
		switch (opt) {
			case 'i': {
				int id;
				if (!GetAsInt(optarg, id)) {
					cerr << "Error: Invalid device ID " << optarg << endl;
					exit(EXIT_FAILURE);
				}
				device->set_id(id);
				break;
			}

			case 'u': {
				int unit;
				if (!GetAsInt(optarg, unit)) {
					cerr << "Error: Invalid unit " << optarg << endl;
					exit(EXIT_FAILURE);
				}
				device->set_unit(unit);
				break;
			}

			case 'C':
				command.set_operation(CREATE_IMAGE);
				image_params = optarg;
				break;

			case 'b':
				int block_size;
				if (!GetAsInt(optarg, block_size)) {
					cerr << "Error: Invalid block size " << optarg << endl;
					exit(EXIT_FAILURE);
				}
				device->set_block_size(block_size);
				break;

			case 'c':
				command.set_operation(ParseOperation(optarg));
				if (command.operation() == NONE) {
					cerr << "Error: Unknown operation '" << optarg << "'" << endl;
					exit(EXIT_FAILURE);
				}
				break;

			case 'D':
				command.set_operation(DETACH_ALL);
				break;

			case 'd':
				command.set_operation(DELETE_IMAGE);
				image_params = optarg;
				break;

			case 'E':
				command.set_operation(IMAGE_FILE_INFO);
				filename = optarg;
				break;

			case 'e':
				command.set_operation(DEFAULT_IMAGE_FILES_INFO);
				break;

			case 'F':
				command.set_operation(DEFAULT_FOLDER);
				default_folder = optarg;
				break;

			case 'f':
				param = optarg;
				break;

			case 'h':
				hostname = optarg;
				break;

			case 'I':
				command.set_operation(RESERVED_IDS_INFO);
				break;

			case 'L':
				command.set_operation(LOG_LEVEL);
				log_level = optarg;
				break;

			case 'l':
				list = true;
				break;

			case 'm':
				command.set_operation(MAPPING_INFO);
				break;

			case 'N':
				command.set_operation(NETWORK_INTERFACES_INFO);
				break;

			case 'O':
				command.set_operation(LOG_LEVEL_INFO);
				break;

			case 't':
				device->set_type(ParseType(optarg));
				if (device->type() == UNDEFINED) {
					cerr << "Error: Unknown device type '" << optarg << "'" << endl;
					exit(EXIT_FAILURE);
				}
				break;

			case 'r':
				command.set_operation(RESERVE_IDS);
				reserved_ids = optarg;
				break;

			case 'R':
				command.set_operation(RENAME_IMAGE);
				image_params = optarg;
				break;

			case 'n': {
					string vendor;
					string product;
					string revision;

					string s = optarg;
					size_t separator_pos = s.find(COMPONENT_SEPARATOR);
					if (separator_pos != string::npos) {
						vendor = s.substr(0, separator_pos);
						s = s.substr(separator_pos + 1);
						separator_pos = s.find(COMPONENT_SEPARATOR);
						if (separator_pos != string::npos) {
							product = s.substr(0, separator_pos);
							revision = s.substr(separator_pos + 1);
						}
						else {
							product = s;
						}
					}
					else {
						vendor = s;
					}

					device->set_vendor(vendor);
					device->set_product(product);
					device->set_revision(revision);
				}
				break;

			case 'p':
				if (!GetAsInt(optarg, port) || port <= 0 || port > 65535) {
					cerr << "Error: Invalid port " << optarg << ", port must be between 1 and 65535" << endl;
					exit(EXIT_FAILURE);
				}
				break;

			case 's':
				command.set_operation(SERVER_INFO);
				break;

			case 'v':
				cout << "rasctl version: " << rascsi_get_version_string() << endl;
				exit(EXIT_SUCCESS);
				break;

			case 'V':
				command.set_operation(VERSION_INFO);
				break;

			case 'x':
				command.set_operation(COPY_IMAGE);
				image_params = optarg;
				break;

			case 'T':
				command.set_operation(DEVICE_TYPES_INFO);
				break;

			case 'X':
				command.set_operation(SHUT_DOWN);
				break;
		}
	}

	if (optopt) {
		exit(EXIT_FAILURE);
	}

	// Listing devices is a special case (rasctl backwards compatibility)
	if (list) {
		PbCommand command_list;
		command_list.set_operation(DEVICES_INFO);
		RasctlCommands rasctl_commands(command_list, hostname, port);
		rasctl_commands.CommandDevicesInfo();
		exit(EXIT_SUCCESS);
	}

	if (!param.empty()) {
		// Only one of these parameters will be used, depending on the device type
		AddParam(*device, "interfaces", param);
		AddParam(*device, "file", param);
	}

	RasctlCommands rasctl_commands(command, hostname, port);

	switch(command.operation()) {
		case LOG_LEVEL:
			rasctl_commands.CommandLogLevel(log_level);
			break;

		case DEFAULT_FOLDER:
			rasctl_commands.CommandDefaultImageFolder(default_folder);
			break;

		case RESERVE_IDS:
			rasctl_commands.CommandReserveIds(reserved_ids);
			break;

		case CREATE_IMAGE:
			rasctl_commands.CommandCreateImage(image_params);
			break;

		case DELETE_IMAGE:
			rasctl_commands.CommandDeleteImage(image_params);
			break;

		case RENAME_IMAGE:
			rasctl_commands.CommandRenameImage(image_params);
			break;

		case COPY_IMAGE:
			rasctl_commands.CommandCopyImage(image_params);
			break;

		case DEVICES_INFO:
			rasctl_commands.CommandDeviceInfo();
			break;

		case DEVICE_TYPES_INFO:
			rasctl_commands.CommandDeviceTypesInfo();
			break;

		case VERSION_INFO:
			rasctl_commands.CommandVersionInfo();
			break;

		case SERVER_INFO:
			rasctl_commands.CommandServerInfo();
			break;

		case DEFAULT_IMAGE_FILES_INFO:
			rasctl_commands.CommandDefaultImageFilesInfo();
			break;

		case IMAGE_FILE_INFO:
			rasctl_commands.CommandImageFileInfo(filename);
			break;

		case NETWORK_INTERFACES_INFO:
			rasctl_commands.CommandNetworkInterfacesInfo();
			break;

		case LOG_LEVEL_INFO:
			rasctl_commands.CommandLogLevelInfo();
			break;

		case RESERVED_IDS_INFO:
			rasctl_commands.CommandReservedIdsInfo();
			break;

		case MAPPING_INFO:
			rasctl_commands.CommandMappingInfo();
			break;

		default:
			rasctl_commands.SendCommand();
			break;
	}

	exit(EXIT_SUCCESS);
}

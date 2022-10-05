//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//	Copyright (C) 2020-2021 Contributors to the RaSCSI project
//	[ Send Control Command ]
//
//---------------------------------------------------------------------------

#include "rascsi_version.h"
#include "protobuf_util.h"
#include "rasutil.h"
#include "rascsi_interface.pb.h"
#include "rasctl/rasctl_parser.h"
#include "rasctl/rasctl_commands.h"
#include <unistd.h>
#include <clocale>
#include <iostream>
#include <list>

// Separator for the INQUIRY name components and for compound parameters
static const char COMPONENT_SEPARATOR = ':';

using namespace std;
using namespace rascsi_interface;
using namespace ras_util;
using namespace protobuf_util;

void SetPatternParams(PbCommand& command, string_view patterns)
{
	string folder_pattern;
	string file_pattern;
	if (const size_t separator_pos = patterns.find(COMPONENT_SEPARATOR); separator_pos != string::npos) {
		folder_pattern = patterns.substr(0, separator_pos);
		file_pattern = patterns.substr(separator_pos + 1);
	}
	else {
		file_pattern = patterns;
	}

	AddParam(command, "folder_pattern", folder_pattern);
	AddParam(command, "file_pattern", file_pattern);
}

int main(int argc, char* argv[])
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	// Display help
	if (argc < 2) {
		cerr << "SCSI Target Emulator RaSCSI Controller\n";
		cerr << "version " << rascsi_get_version_string() << " (" << __DATE__ << ", " << __TIME__ << ")\n";
		cerr << "Usage: " << argv[0] << " -i ID [-u UNIT] [-c CMD] [-C FILE] [-t TYPE] [-b BLOCK_SIZE] [-n NAME] [-f FILE|PARAM] ";
		cerr << "[-F IMAGE_FOLDER] [-L LOG_LEVEL] [-h HOST] [-p PORT] [-r RESERVED_IDS] ";
		cerr << "[-C FILENAME:FILESIZE] [-d FILENAME] [-w FILENAME] [-R CURRENT_NAME:NEW_NAME] ";
		cerr <<	"[-x CURRENT_NAME:NEW_NAME] [-z LOCALE] ";
		cerr << "[-e] [-E FILENAME] [-D] [-I] [-l] [-L] [-m] [o] [-O] [-P] [-s] [-v] [-V] [-y] [-X]\n";
		cerr << " where  ID := {0-7}\n";
		cerr << "        UNIT := {0-31}, default is 0\n";
		cerr << "        CMD := {attach|detach|insert|eject|protect|unprotect|show}\n";
		cerr << "        TYPE := {schd|scrm|sccd|scmo|scbr|scdp} or convenience type {hd|rm|mo|cd|bridge|daynaport}\n";
		cerr << "        BLOCK_SIZE := {512|1024|2048|4096) bytes per hard disk drive block\n";
		cerr << "        NAME := name of device to attach (VENDOR:PRODUCT:REVISION)\n";
		cerr << "        FILE|PARAM := image file path or device-specific parameter\n";
		cerr << "        IMAGE_FOLDER := default location for image files, default is '~/images'\n";
		cerr << "        HOST := rascsi host to connect to, default is 'localhost'\n";
		cerr << "        PORT := rascsi port to connect to, default is 6868\n";
		cerr << "        RESERVED_IDS := comma-separated list of IDs to reserve\n";
		cerr << "        LOG_LEVEL := log level {trace|debug|info|warn|err|critical|off}, default is 'info'\n";
		cerr << " If CMD is 'attach' or 'insert' the FILE parameter is required.\n";
		cerr << "Usage: " << argv[0] << " -l\n";
		cerr << "       Print device list.\n" << flush;

		exit(EXIT_SUCCESS);
	}

	RasctlParser parser;
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
	string token;
	bool list = false;

	const char *locale = setlocale(LC_MESSAGES, "");
	if (locale == nullptr || !strcmp(locale, "C")) {
		locale = "en";
	}

	opterr = 1;
	int opt;
	while ((opt = getopt(argc, argv, "e::lmos::vDINOTVXa:b:c:d:f:h:i:n:p:r:t:u:x:z:C:E:F:L:P::R:")) != -1) {
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
				command.set_operation(parser.ParseOperation(optarg));
				if (command.operation() == NO_OPERATION) {
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
                if (optarg) {
                	SetPatternParams(command, optarg);
                }
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

			case 'o':
				command.set_operation(OPERATION_INFO);
				break;

			case 't':
				device->set_type(parser.ParseType(optarg));
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
					if (size_t separator_pos = s.find(COMPONENT_SEPARATOR); separator_pos != string::npos) {
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
                if (optarg) {
                	SetPatternParams(command, optarg);
                }
                break;

			case 'v':
				cout << "rasctl version: " << rascsi_get_version_string() << endl;
				exit(EXIT_SUCCESS);
				break;

			case 'P':
				token = optarg ? optarg : getpass("Password: ");
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
				AddParam(command, "mode", "rascsi");
				break;

			case 'z':
				locale = optarg;
				break;

			default:
				break;
		}
	}

	// For macos only 'if (optind != argc)' appears to work, but then non-argument options do not reject arguments
	if (optopt) {
		exit(EXIT_FAILURE);
	}

	// Listing devices is a special case (rasctl backwards compatibility)
	if (list) {
		PbCommand command_list;
		command_list.set_operation(DEVICES_INFO);
		RasctlCommands rasctl_commands(command_list, hostname, port, token, locale);
		rasctl_commands.CommandDevicesInfo();
		exit(EXIT_SUCCESS);
	}

	ParseParameters(*device, param);

	RasctlCommands rasctl_commands(command, hostname, port, token, locale);
	rasctl_commands.Execute(log_level, default_folder, reserved_ids, image_params, filename);

	exit(EXIT_SUCCESS);
}

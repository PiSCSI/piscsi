//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Powered by XM6 TypeG Technology.
// Copyright (C) 2016-2020 GIMONS
// Copyright (C) 2020-2023 Contributors to the PiSCSI project
// Copyright (C) 2021-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "controllers/controller_manager.h"
#include "controllers/scsi_controller.h"
#include "shared/piscsi_util.h"
#include "shared/protobuf_util.h"
#include "shared/piscsi_exceptions.h"
#include "shared/piscsi_version.h"
#include "scsictl/scsictl_parser.h"
#include "scsictl/scsictl_commands.h"
#include "scsictl/scsictl_core.h"
#include <google/protobuf/util/json_util.h>
#include <google/protobuf/text_format.h>
#include <unistd.h>
#include <clocale>
#include <iostream>
#include <fstream>

using namespace std;
using namespace google::protobuf;
using namespace google::protobuf::util;
using namespace piscsi_interface;
using namespace piscsi_util;
using namespace protobuf_util;

void ScsiCtl::Banner(const vector<char *>& args) const
{
	if (args.size() < 2) {
		cout << piscsi_util::Banner("(Controller App)")
				<< "\nUsage: " << args[0] << " -i ID[:LUN] [-c CMD] [-C FILE] [-t TYPE] [-b BLOCK_SIZE] [-n NAME] [-f FILE|PARAM] "
				<< "[-F IMAGE_FOLDER] [-L LOG_LEVEL] [-h HOST] [-p PORT] [-r RESERVED_IDS] "
            << "[-C FILENAME:FILESIZE] [-d FILENAME] [-B FILENAME] [-J FILENAME] [-T FILENAME] [-R CURRENT_NAME:NEW_NAME] "
				<< "[-x CURRENT_NAME:NEW_NAME] [-z LOCALE] "
				<< "[-e] [-E FILENAME] [-D] [-I] [-l] [-m] [o] [-O] [-P] [-s] [-S] [-v] [-V] [-y] [-X]\n"
				<< " where  ID[:LUN] ID := {0-" << (ControllerManager::GetScsiIdMax() - 1) << "},"
				<< " LUN := {0-" << (ControllerManager::GetScsiLunMax() - 1) << "}, default is 0\n"
				<< "        CMD := {attach|detach|insert|eject|protect|unprotect|show}\n"
				<< "        TYPE := {schd|scrm|sccd|scmo|scbr|scdp} or convenience type {hd|rm|mo|cd|bridge|daynaport}\n"
				<< "        BLOCK_SIZE := {512|1024|2048|4096) bytes per hard disk drive block\n"
				<< "        NAME := name of device to attach (VENDOR:PRODUCT:REVISION)\n"
				<< "        FILE|PARAM := image file path or device-specific parameter\n"
				<< "        IMAGE_FOLDER := default location for image files, default is '~/images'\n"
				<< "        HOST := piscsi host to connect to, default is 'localhost'\n"
				<< "        PORT := piscsi port to connect to, default is 6868\n"
				<< "        RESERVED_IDS := comma-separated list of IDs to reserve\n"
				<< "        LOG_LEVEL := log level {trace|debug|info|warn|err|off}, default is 'info'\n"
				<< " If CMD is 'attach' or 'insert' the FILE parameter is required.\n"
				<< "Usage: " << args[0] << " -l\n"
				<< "       Print device list.\n" << flush;

		exit(EXIT_SUCCESS);
	}
}

int ScsiCtl::run(const vector<char *>& args) const
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	Banner(args);

	ScsictlParser parser;
	PbCommand command;
	PbDeviceDefinition* device = command.add_devices();
	device->set_id(-1);
    string hostname = "localhost";
	int port = 6868;
	string param;
	string log_level;
	string default_folder;
	string reserved_ids;
	string image_params;
	string filename;
	string filename_json;
    string filename_binary;
    string filename_text;
 	string token;
	bool list = false;

	string locale = GetLocale();

	opterr = 1;
	int opt;
	while ((opt = getopt(static_cast<int>(args.size()), args.data(),
        "e::lmos::vDINOSTVXa:b:c:d:f:h:i:n:p:r:t:x:z:B:C:E:F:J:L:P::R:Z:")) != -1) {
        switch (opt) {
        case 'i':
            if (const string error = SetIdAndLun(*device, optarg); !error.empty()) {
                cerr << "Error: " << error << endl;
                exit(EXIT_FAILURE);
            }
            break;

        case 'C':
            command.set_operation(CREATE_IMAGE);
            image_params = optarg;
            break;

        case 'b':
            int block_size;
            if (!GetAsUnsignedInt(optarg, block_size)) {
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
            filename = optarg;
            if (filename.empty()) {
                cerr << "Error: Missing filename" << endl;
                exit(EXIT_FAILURE);
            }
            command.set_operation(IMAGE_FILE_INFO);
            break;

        case 'e':
            command.set_operation(DEFAULT_IMAGE_FILES_INFO);
            if (optarg) {
                SetCommandParams(command, optarg);
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
            if (hostname.empty()) {
                cerr << "Error: Missing hostname" << endl;
                exit(EXIT_FAILURE);
            }
            break;

        case 'B':
            filename_binary = optarg;
            if (filename_binary.empty()) {
                cerr << "Error: Missing filename" << endl;
                exit(EXIT_FAILURE);
            }
            break;

        case 'J':
            filename_json = optarg;
            if (filename_json.empty()) {
                cerr << "Error: Missing filename" << endl;
                exit(EXIT_FAILURE);
            }
            break;

        case 'Z':
            filename_text = optarg;
            if (filename_text.empty()) {
                cerr << "Error: Missing filename" << endl;
                exit(EXIT_FAILURE);
            }
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

        case 'n':
            SetProductData(*device, optarg);
            break;

        case 'p':
            if (!GetAsUnsignedInt(optarg, port) || port <= 0 || port > 65535) {
                cerr << "Error: Invalid port " << optarg << ", port must be between 1 and 65535" << endl;
                exit(EXIT_FAILURE);
            }
            break;

        case 's':
            command.set_operation(SERVER_INFO);
            if (optarg) {
                if (const string error = SetCommandParams(command, optarg); !error.empty()) {
                    cerr << "Error: " << error << endl;
                    exit(EXIT_FAILURE);
                }
            }
            break;

        case 'S':
            command.set_operation(STATISTICS_INFO);
            break;

        case 'v':
            cout << "scsictl version: " << piscsi_get_version_string() << '\n';
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
            SetParam(command, "mode", "rascsi");
            break;

        case 'z':
            locale = optarg;
            break;

        default:
            break;
        }
    }

	// For macos only 'optind != argc' appears to work, but then non-argument options do not reject arguments
	if (optopt) {
		exit(EXIT_FAILURE);
	}

    if (!filename_json.empty()) {
        return ExportAsJson(command, filename_json);
    }
    if (!filename_binary.empty()) {
        return ExportAsBinary(command, filename_binary);
    }
    if (!filename_text.empty()) {
        return ExportAsText(command, filename_text);
    }

    SetParam(command, "token", token);
    SetParam(command, "locale", locale);

	ScsictlCommands scsictl_commands(command, hostname, port);

	bool status;
	try {
		// Listing devices is a special case (legacy rasctl backwards compatibility)
		if (list) {
			command.clear_devices();
			command.set_operation(DEVICES_INFO);

			status = scsictl_commands.CommandDevicesInfo();
		}
		else {
			ParseParameters(*device, param);

            status = scsictl_commands.Execute(log_level, default_folder, reserved_ids, image_params, filename);
		}
	}
    catch(const io_exception& e) {
    	cerr << "Error: " << e.what() << endl;

    	status = false;

    	// Fall through
	}

    return status ? EXIT_SUCCESS : EXIT_FAILURE;
}

int ScsiCtl::ExportAsBinary(const PbCommand &command, const string &filename) const
{
    const string binary = command.SerializeAsString();

    ofstream out;
    out.open(filename, ios::binary);
    out << binary;
    if (out.fail()) {
        cerr << "Error: Can't create protobuf binary file '" << filename << "'" << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int ScsiCtl::ExportAsJson(const PbCommand &command, const string &filename) const
{
    string json;
    MessageToJsonString(command, &json);

    ofstream out(filename);
    out << json;
    if (out.fail()) {
        cerr << "Error: Can't create protobuf JSON file '" << filename << "'" << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int ScsiCtl::ExportAsText(const PbCommand &command, const string &filename) const
{
    string text;
    TextFormat::PrintToString(command, &text);

    ofstream out(filename);
    out << text;
    if (out.fail()) {
        cerr << "Error: Can't create protobuf text format file '" << filename << "'" << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

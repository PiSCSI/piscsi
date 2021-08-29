//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//	[ Send Control Command ]
//
//---------------------------------------------------------------------------

#include <netdb.h>
#include "os.h"
#include "rascsi_version.h"
#include "exceptions.h"
#include "protobuf_util.h"
#include "rasutil.h"
#include "rascsi_interface.pb.h"
#include <sstream>
#include <iostream>
#include <list>

// Separator for the INQUIRY name components
#define COMPONENT_SEPARATOR ':'

using namespace std;
using namespace rascsi_interface;

//---------------------------------------------------------------------------
//
//	Send Command
//
//---------------------------------------------------------------------------
int SendCommand(const string& hostname, int port, const PbCommand& command)
{
	int fd = -1;

	try {
    	struct hostent *host = gethostbyname(hostname.c_str());
    	if (!host) {
    		throw io_exception("Can't resolve hostname '" + hostname + "'");
    	}

    	fd = socket(AF_INET, SOCK_STREAM, 0);
    	if (fd < 0) {
    		throw io_exception("Can't create socket");
    	}

    	struct sockaddr_in server;
    	memset(&server, 0, sizeof(server));
    	server.sin_family = AF_INET;
    	server.sin_port = htons(port);
    	server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    	memcpy(&server.sin_addr.s_addr, host->h_addr, host->h_length);

    	if (connect(fd, (struct sockaddr *)&server, sizeof(struct sockaddr_in)) < 0) {
    		ostringstream error;
    		error << "Can't connect to rascsi process on host '" << hostname << "', port " << port;
    		throw io_exception(error.str());
    	}

        SerializeMessage(fd, command);
    }
    catch(const io_exception& e) {
    	cerr << "Error: " << e.getmsg() << endl;

        if (fd >= 0) {
        	close(fd);
        }

        exit(fd < 0 ? ENOTCONN : EXIT_FAILURE);
    }

    return fd;
}

//---------------------------------------------------------------------------
//
//	Receive command result
//
//---------------------------------------------------------------------------
bool ReceiveResult(int fd)
{
    try {
        PbResult result;
        DeserializeMessage(fd, result);
        close(fd);

    	if (!result.status()) {
    		throw io_exception(result.msg());
    	}

    	if (!result.msg().empty()) {
    		cout << result.msg() << endl;
    	}
    }
    catch(const io_exception& e) {
    	cerr << "Error: " << e.getmsg() << endl;

    	return false;
    }

    return true;
}

//---------------------------------------------------------------------------
//
//	Command implementations
//
//---------------------------------------------------------------------------

const PbServerInfo GetServerInfo(const string&hostname, int port) {
	PbCommand command;
	command.set_operation(SERVER_INFO);

	int fd = SendCommand(hostname.c_str(), port, command);

	PbServerInfo serverInfo;
	try {
		DeserializeMessage(fd, serverInfo);
	}
	catch(const io_exception& e) {
		cerr << "Error: " << e.getmsg() << endl;

		close(fd);

		exit(EXIT_FAILURE);
	}

	close(fd);

	return serverInfo;
}

void CommandList(const string& hostname, int port)
{
	PbServerInfo serverInfo = GetServerInfo(hostname, port);

	cout << ListDevices(serverInfo.devices()) << endl;
}

void CommandLogLevel(const string& hostname, int port, const string& log_level)
{
	PbCommand command;
	command.set_operation(LOG_LEVEL);
	command.set_params(log_level);

	int fd = SendCommand(hostname.c_str(), port, command);
	ReceiveResult(fd);
	close(fd);
}

void CommandDefaultImageFolder(const string& hostname, int port, const string& folder)
{
	PbCommand command;
	command.set_operation(DEFAULT_FOLDER);
	command.set_params(folder);

	int fd = SendCommand(hostname.c_str(), port, command);
	ReceiveResult(fd);
	close(fd);
}

void CommandServerInfo(const string& hostname, int port)
{
	PbCommand command;
	command.set_operation(SERVER_INFO);

	int fd = SendCommand(hostname.c_str(), port, command);

	PbServerInfo serverInfo;
	try {
		DeserializeMessage(fd, serverInfo);
	}
	catch(const io_exception& e) {
		cerr << "Error: " << e.getmsg() << endl;

		close(fd);

		exit(EXIT_FAILURE);
	}

	close(fd);

	cout << "rascsi server version: " << serverInfo.rascsi_version() << endl;

	if (!serverInfo.log_levels_size()) {
		cout << "  No log level settings available" << endl;
	}
	else {
		cout << "rascsi log levels, sorted by severity:" << endl;
		for (int i = 0; i < serverInfo.log_levels_size(); i++) {
			cout << "  " << serverInfo.log_levels(i) << endl;
		}

		cout << "Current rascsi log level: " << serverInfo.current_log_level() << endl;
	}

	cout << "Default image file folder: " << serverInfo.default_image_folder() << endl;
	if (!serverInfo.image_files_size()) {
		cout << "  No image files available" << endl;
	}
	else {
		list<PbImageFile> files = { serverInfo.image_files().begin(), serverInfo.image_files().end() };
		files.sort([](const PbImageFile& a, const PbImageFile& b) { return a.name() < b.name(); });

		cout << "Available image files:" << endl;
		for (const auto& file : files) {
			cout << "  " << file.name() << " (" << file.size() << " bytes)";
			if (file.read_only()) {
				cout << ", read-only";
			}
			cout << endl;
		}
	}

	cout << "Supported device types and their properties:" << endl;
	for (auto it = serverInfo.types_properties().begin(); it != serverInfo.types_properties().end(); ++it) {
		cout << "  " << PbDeviceType_Name(it->type());

		cout << "  Properties: ";
		bool has_property = false;
		const PbDeviceProperties& properties = it->properties();
		if (properties.read_only()) {
			cout << "Read-only";
					has_property = true;
		}
		if (properties.protectable()) {
			cout << (has_property ? ", " : "") << "Protectable";
			has_property = true;
		}
		if (properties.removable()) {
			cout << (has_property ? ", " : "") << "Removable";
			has_property = true;
		}
		if (properties.lockable()) {
			cout << (has_property ? ", " : "") << "Lockable";
			has_property = true;
		}
		if (properties.supports_file()) {
			cout << (has_property ? ", " : "") << "Image file support";
			has_property = true;
		}
		else if (properties.supports_params()) {
			cout << (has_property ? ", " : "") << "Parameter support";
			has_property = true;
		}

		if (!has_property) {
			cout << "None";
		}
		cout << endl;

		if (properties.block_sizes_size()) {
			list<uint32_t> block_sizes = { properties.block_sizes().begin(), properties.block_sizes().end() };
			block_sizes.sort([](const auto& a, const auto& b) { return a < b; });

			cout << "        Configurable block sizes in bytes: ";

			bool isFirst = true;
			for (const auto& block_size : block_sizes) {
				if (!isFirst) {
					cout << ", ";
				}
				cout << block_size;

				isFirst = false;
			}
			cout << endl;
		}

		if (properties.capacities_size()) {
			list<uint64_t> capacities = { properties.capacities().begin(), properties.capacities().end() };
			capacities.sort([](const auto& a, const auto& b) { return a < b; });

			cout << "        Media capacities in bytes: ";

			bool isFirst = true;
			for (const auto& capacity : capacities) {
				if (!isFirst) {
					cout << ", ";
				}
				cout << capacity;

				isFirst = false;
			}
			cout << endl;
		}
	}

	if (!serverInfo.devices().devices_size()) {
		cout << "No devices attached" << endl;
	}
	else {
		list<PbDevice> sorted_devices = { serverInfo.devices().devices().begin(), serverInfo.devices().devices().end() };
		sorted_devices.sort([](const PbDevice& a, const PbDevice& b) { return a.id() < b.id(); });

		cout << "Attached devices:" << endl;

		for (const auto& device : sorted_devices) {
			cout << "  " << device.id() << ":" << device.unit() << "  " << PbDeviceType_Name(device.type())
					<< "  " << device.vendor() << ":" << device.product() << ":" << device.revision();
			if (device.block_size()) {
				cout << "  " << device.block_size() << " BPS";
			}
			cout << (device.file().name().empty() ? "" : "  " + device.file().name());
			if (device.properties().read_only() || device.status().protected_()) {
				cout << "  read-only";
			}
			cout <<endl;
		}
	}
}

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
		// Parse legacy types
		switch (tolower(optarg[0])) {
			case 'm':
				return SCMO;

			case 'c':
				return SCCD;

			case 'b':
				return SCBR;
		}
	}

	return UNDEFINED;
}

//---------------------------------------------------------------------------
//
//	Main processing
//
//---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	// Display help
	if (argc < 2) {
		cerr << "SCSI Target Emulator RaSCSI Controller" << endl;
		cerr << "version " << rascsi_get_version_string() << " (" << __DATE__ << ", " << __TIME__ << ")" << endl;
		cerr << "Usage: " << argv[0] << " -i ID [-u UNIT] [-c CMD] [-t TYPE] [-b BLOCK_SIZE] [-n NAME] [-f FILE] [-d DEFAULT_IMAGE_FOLDER] [-g LOG_LEVEL] [-h HOST] [-p PORT] [-l] [-v]" << endl;
		cerr << " where  ID := {0|1|2|3|4|5|6|7}" << endl;
		cerr << "        UNIT := {0|1}, default setting is 0." << endl;
		cerr << "        CMD := {attach|detach|insert|eject|protect|unprotect}" << endl;
		cerr << "        TYPE := {sahd|schd|scrm|sccd|scmo|scbr|scdp} or legacy types {hd|mo|cd|bridge}" << endl;
		cerr << "        BLOCK_SIZE := {512|1024|2048|4096) bytes per hard disk drive block" << endl;
		cerr << "        NAME := name of device to attach (VENDOR:PRODUCT:REVISION)" << endl;
		cerr << "        FILE := image file path" << endl;
		cerr << "        DEFAULT_IMAGE_FOLDER := default location for image files, default is '~/images'" << endl;
		cerr << "        HOST := rascsi host to connect to, default is 'localhost'" << endl;
		cerr << "        PORT := rascsi port to connect to, default is 6868" << endl;
		cerr << "        LOG_LEVEL := log level {trace|debug|info|warn|err|critical|off}, default is 'trace'" << endl;
		cerr << " If CMD is 'attach' or 'insert' the FILE parameter is required." << endl;
		cerr << "Usage: " << argv[0] << " -l" << endl;
		cerr << "       Print device list." << endl;

		exit(EXIT_SUCCESS);
	}

	// Parse the arguments
	PbCommand command;
	PbDeviceDefinitions devices;
	command.set_allocated_devices(&devices);
	PbDeviceDefinition *device = devices.add_devices();
	device->set_id(-1);
	const char *hostname = "localhost";
	int port = 6868;
	string log_level;
	string default_folder;
	bool list = false;

	opterr = 1;
	int opt;
	while ((opt = getopt(argc, argv, "b:c:d:f:g:h:i:n:p:t:u:lsv")) != -1) {
		switch (opt) {
			case 'i':
				device->set_id(optarg[0] - '0');
				break;

			case 'u':
				device->set_unit(optarg[0] - '0');
				break;

			case 'b':
				device->set_block_size(atoi(optarg));
				break;

			case 'c':
				command.set_operation(ParseOperation(optarg));
				break;

			case 'd':
				command.set_operation(DEFAULT_FOLDER);
				default_folder = optarg;
				break;

			case 'f':
				device->set_params(optarg);
				break;

			case 't':
				device->set_type(ParseType(optarg));
				if (device->type() == UNDEFINED) {
					cerr << "Error: Unknown device type '" << optarg << "'" << endl;
					exit(EXIT_FAILURE);
				}
				break;

			case 'g':
				command.set_operation(LOG_LEVEL);
				log_level = optarg;
				break;

			case 'h':
				hostname = optarg;
				break;

			case 'l':
				list = true;
				break;

			case 'n': {
					string vendor;
					string product;
					string revision;

					string s = optarg;
					size_t separatorPos = s.find(COMPONENT_SEPARATOR);
					if (separatorPos != string::npos) {
						vendor = s.substr(0, separatorPos);
						s = s.substr(separatorPos + 1);
						separatorPos = s.find(COMPONENT_SEPARATOR);
						if (separatorPos != string::npos) {
							product = s.substr(0, separatorPos);
							revision = s.substr(separatorPos + 1);
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
				port = atoi(optarg);
				if (port <= 0 || port > 65535) {
					cerr << "Error: Invalid port " << optarg << ", port must be between 1 and 65535" << endl;
					exit(EXIT_FAILURE);
				}
				break;

			case 's':
				command.set_operation(SERVER_INFO);
				break;

			case 'v':
				cout << rascsi_get_version_string() << endl;
				exit(EXIT_SUCCESS);
				break;
		}
	}

	if (optopt) {
		exit(EXIT_FAILURE);
	}

	if (command.operation() == LOG_LEVEL) {
		CommandLogLevel(hostname, port, log_level);
		exit(EXIT_SUCCESS);
	}

	if (command.operation() == DEFAULT_FOLDER) {
		CommandDefaultImageFolder(hostname, port, default_folder);
		exit(EXIT_SUCCESS);
	}

	if (command.operation() == SERVER_INFO) {
		CommandServerInfo(hostname, port);
		exit(EXIT_SUCCESS);
	}

	if (list) {
		CommandList(hostname, port);
		exit(EXIT_SUCCESS);
	}

	// Send the command
	int fd = SendCommand(hostname, port, command);
	bool status = ReceiveResult(fd);
	close(fd);

	// All done!
	exit(status ? EXIT_SUCCESS : EXIT_FAILURE);
}

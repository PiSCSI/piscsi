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
	command.set_cmd(SERVER_INFO);

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
	command.set_cmd(LOG_LEVEL);
	command.set_params(log_level);

	int fd = SendCommand(hostname.c_str(), port, command);
	ReceiveResult(fd);
	close(fd);
}

void CommandDefaultImageFolder(const string& hostname, int port, const string& folder)
{
	PbCommand command;
	command.set_cmd(DEFAULT_FOLDER);
	command.set_params(folder);

	int fd = SendCommand(hostname.c_str(), port, command);
	ReceiveResult(fd);
	close(fd);
}

void CommandServerInfo(const string& hostname, int port)
{
	PbCommand command;
	command.set_cmd(SERVER_INFO);

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

	if (!serverInfo.available_log_levels_size()) {
		cout << "  No log level settings available" << endl;
	}
	else {
		cout << "rascsi log levels, sorted by severity:" << endl;
		for (int i = 0; i < serverInfo.available_log_levels_size(); i++) {
			cout << "  " << serverInfo.available_log_levels(i) << endl;
		}

		cout << "Current rascsi log level: " << serverInfo.current_log_level() << endl;
	}

	cout << "Default image file folder: " << serverInfo.default_image_folder() << endl;
	if (!serverInfo.image_files_size()) {
		cout << "  No image files available in the default folder" << endl;
	}
	else {
		list<PbImageFile> sorted_files;
		for (int i = 0; i < serverInfo.image_files_size(); i++) {
			sorted_files.push_back(serverInfo.image_files(i));
		}
		sorted_files.sort([](const PbImageFile& a, const PbImageFile& b) { return a.name() < b.name(); });

		cout << "Image files available in the default folder:" << endl;
		for (auto it = sorted_files.begin(); it != sorted_files.end(); ++it) {
			const PbImageFile& file = *it;

			cout << "  " << file.name() << " (" << file.size() << " bytes)" << (file.read_only() ? ", read-only": "")
					<< endl;
		}
	}

	cout << "Device types and their properties:" << endl;
	for (int i = 0; i < serverInfo.types_properties_size(); i++) {
		PbDeviceTypeProperties types_properties = serverInfo.types_properties(i);
		cout << "  " << PbDeviceType_Name(types_properties.type());

		vector<int> block_sizes;

		cout << "  Properties: ";
		if (types_properties.properties_size()) {
			for (int j = 0; j < types_properties.properties_size(); j++) {
				bool has_property = false;

				PbDeviceProperties properties = types_properties.properties(j);
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
				}
				cout << endl;

				for (int k = 0 ; k < properties.block_sizes_size(); k++)
				{
					block_sizes.push_back(properties.block_sizes(i));
				}
			}
		}
		else {
			cout << "None" << endl;
		}

		if (block_sizes.empty()) {
			cout << "        Block size is not configurable" << endl;
		}
		else {
			cout << "        Configurable block sizes: ";
			for (size_t j = 0; j < block_sizes.size(); j++) {
				if (j) {
					cout << ", ";
				}
				cout << block_sizes.at(j);
			}
			cout << endl;
		}
	}

	cout << "Available devices:" << endl;
	const PbDevices& devices = serverInfo.devices();
	if (!devices.devices_size()) {
		cout << "No devices available" << endl;
	}
	else {
		list<PbDevice> sorted_devices;
		for (int i = 0; i < devices.devices_size(); i++) {
			sorted_devices.push_back(devices.devices(i));
		}
		sorted_devices.sort([](const PbDevice& a, const PbDevice& b) { return a.id() < b.id(); });

		for (auto it = sorted_devices.begin(); it != sorted_devices.end(); ++it) {
			const PbDevice& device = *it;

			cout << "  " << device.id() << ":" << device.unit() << "  " << PbDeviceType_Name(device.type())
					<< "  " << device.vendor() << ":" << device.product() << ":" << device.revision();
			if (device.block_size()) {
				cout << "  " << device.block_size() << " BPS";
			}
			cout << (device.file().name().empty() ? "" : "  " + device.file().name()) << endl;
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
				command.set_cmd(ParseOperation(optarg));
				break;

			case 'd':
				command.set_cmd(DEFAULT_FOLDER);
				default_folder = optarg;
				break;

			case 'f':
				device->set_params(optarg);
				break;

			case 't':
				device->set_type(ParseType(optarg));
				break;

			case 'g':
				command.set_cmd(LOG_LEVEL);
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
					size_t separatorPos = s.find(':');
					if (separatorPos != string::npos) {
						vendor = s.substr(0, separatorPos);
						s = s.substr(separatorPos + 1);
						separatorPos = s.find(':');
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
					cerr << "Invalid port " << optarg << ", port must be between 1 and 65535" << endl;
					exit(EXIT_FAILURE);
				}
				break;

			case 's':
				command.set_cmd(SERVER_INFO);
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

	if (command.cmd() == LOG_LEVEL) {
		CommandLogLevel(hostname, port, log_level);
		exit(EXIT_SUCCESS);
	}

	if (command.cmd() == DEFAULT_FOLDER) {
		CommandDefaultImageFolder(hostname, port, default_folder);
		exit(EXIT_SUCCESS);
	}

	if (command.cmd() == SERVER_INFO) {
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

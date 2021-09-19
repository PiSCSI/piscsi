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

void SendCommand(const string& hostname, int port, const PbCommand& command, PbResult& result)
{
	// Send command
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

    // Receive result
    try {
        DeserializeMessage(fd, result);

    	if (!result.status()) {
    		throw io_exception(result.msg());
    	}
    }
    catch(const io_exception& e) {
    	close(fd);

    	cerr << "Error: " << e.getmsg() << endl;

    	exit(EXIT_FAILURE);
    }

    close(fd);

	if (!result.msg().empty()) {
		cout << result.msg() << endl;
	}
}

void DisplayDeviceInfo(const PbDevice& pb_device)
{
	cout << "  " << pb_device.id() << ":" << pb_device.unit() << "  " << PbDeviceType_Name(pb_device.type())
			<< "  " << pb_device.vendor() << ":" << pb_device.product() << ":" << pb_device.revision();

	if (pb_device.block_size()) {
		cout << "  " << pb_device.block_size() << " bytes per sector";
		if (pb_device.block_count()) {
			cout << "  " << pb_device.block_size() * pb_device.block_count() << " bytes capacity";
		}
	}

	if (pb_device.properties().supports_file() && !pb_device.file().name().empty()) {
		cout << "  " << pb_device.file().name();
	}

	cout << "  ";
	bool hasProperty = false;
	if (pb_device.properties().read_only()) {
		cout << "read-only";
		hasProperty = true;
	}
	if (pb_device.properties().protectable() && pb_device.status().protected_()) {
		if (hasProperty) {
			cout << ", ";
		}
		cout << "protected";
		hasProperty = true;
	}
	if (pb_device.properties().stoppable() && pb_device.status().stopped()) {
		if (hasProperty) {
			cout << ", ";
		}
		cout << "stopped";
		hasProperty = true;
	}
	if (pb_device.properties().removable() && pb_device.status().removed()) {
		if (hasProperty) {
			cout << ", ";
		}
		cout << "removed";
		hasProperty = true;
	}
	if (pb_device.properties().lockable() && pb_device.status().locked()) {
		if (hasProperty) {
			cout << ", ";
		}
		cout << "locked";
	}
	if (hasProperty) {
		cout << "  ";
	}

	bool isFirst = true;
	for (const auto& param : pb_device.params()) {
		if (!isFirst) {
			cout << "  ";
		}
		isFirst = false;
		cout << param.first << "=" << param.second;
	}

	cout << endl;
}

void DisplayDeviceTypesInfo(const PbDeviceTypesInfo& device_types_info)
{
	cout << "Supported device types and their properties:" << endl;
	for (auto it = device_types_info.properties().begin(); it != device_types_info.properties().end(); ++it) {
		cout << "  " << PbDeviceType_Name(it->type());

		const PbDeviceProperties& properties = it->properties();

		cout << "  Supported LUNs: " << properties.luns() << endl;

		if (properties.read_only() || properties.protectable() || properties.stoppable() || properties.read_only()
				|| properties.lockable()) {
			cout << "        Properties: ";
			bool has_property = false;
			if (properties.read_only()) {
				cout << "read-only";
				has_property = true;
			}
			if (properties.protectable()) {
				cout << (has_property ? ", " : "") << "protectable";
				has_property = true;
			}
			if (properties.stoppable()) {
				cout << (has_property ? ", " : "") << "stoppable";
				has_property = true;
			}
			if (properties.removable()) {
				cout << (has_property ? ", " : "") << "removable";
				has_property = true;
			}
			if (properties.lockable()) {
				cout << (has_property ? ", " : "") << "lockable";
			}
			cout << endl;
		}

		if (properties.supports_file()) {
			cout << "        Image file support" << endl;
		}
		else if (properties.supports_params()) {
			cout << "        Parameter support" << endl;
		}

		if (properties.supports_params() && properties.default_params_size()) {
			map<string, string> params = { properties.default_params().begin(), properties.default_params().end() };

			cout << "        Default parameters: ";

			bool isFirst = true;
			for (const auto& param : params) {
				if (!isFirst) {
					cout << ", ";
				}
				cout << param.first << "=" << param.second;

				isFirst = false;
			}
			cout << endl;
		}

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
}

void DisplayImageFiles(const list<PbImageFile> image_files, const string& default_image_folder)
{
	cout << "Default image file folder: " << default_image_folder << endl;

	if (image_files.empty()) {
		cout << "  No image files available" << endl;
	}
	else {
		list<PbImageFile> files = { image_files.begin(), image_files.end() };
		files.sort([](const auto& a, const auto& b) { return a.name() < b.name(); });

		cout << "Available image files:" << endl;
		for (const auto& file : files) {
			cout << "  " << file.name() << " (" << file.size() << " bytes)";
			if (file.read_only()) {
				cout << ", read-only";
			}
			cout << endl;
		}
	}
}

void DisplayNetworkInterfaces(list<string> interfaces)
{
	interfaces.sort([](const auto& a, const auto& b) { return a < b; });

	cout << "Available (up) network interfaces:" << endl;
	bool isFirst = true;
	for (const auto& interface : interfaces) {
		if (!isFirst) {
			cout << ", ";
		}
		isFirst = false;
		cout << interface;
	}
	cout << endl;
}

//---------------------------------------------------------------------------
//
//	Command implementations
//
//---------------------------------------------------------------------------

void CommandList(const string& hostname, int port)
{
	PbCommand command;
	command.set_operation(DEVICES_INFO);

	PbResult result;
	SendCommand(hostname.c_str(), port, command, result);

	const list<PbDevice>& devices = { result.device_info().devices().begin(), result.device_info().devices().end() };
	cout << ListDevices(devices) << endl;
}

const PbServerInfo GetServerInfo(const PbCommand& command, const string& hostname, int port)
{
	PbResult result;
	SendCommand(hostname.c_str(), port, command, result);

	return result.server_info();
}

void CommandLogLevel(PbCommand& command, const string& hostname, int port, const string& log_level)
{
	AddParam(command, "level", log_level);

	PbResult result;
	SendCommand(hostname.c_str(), port, command, result);
}

void CommandReserve(PbCommand& command, const string&hostname, int port, const string& reserved_ids)
{
	AddParam(command, "ids", reserved_ids);

    PbResult result;
    SendCommand(hostname.c_str(), port, command, result);
}

void CommandCreateImage(PbCommand& command, const string&hostname, int port, const string& image_params)
{
	size_t separatorPos = image_params.find(COMPONENT_SEPARATOR);
	if (separatorPos != string::npos) {
		AddParam(command, "file", image_params.substr(0, separatorPos));
		AddParam(command, "size", image_params.substr(separatorPos + 1));
	}
	else {
		cerr << "Error: Invalid file descriptor '" << image_params << "', format is NAME:SIZE" << endl;
		exit(EXIT_FAILURE);
	}

	AddParam(command, "read_only", "false");

    PbResult result;
    SendCommand(hostname.c_str(), port, command, result);
}

void CommandDeleteImage(PbCommand& command, const string&hostname, int port, const string& filename)
{
	AddParam(command, "file", filename);

    PbResult result;
    SendCommand(hostname.c_str(), port, command, result);
}

void CommandRenameImage(PbCommand& command, const string&hostname, int port, const string& image_params)
{
	size_t separatorPos = image_params.find(COMPONENT_SEPARATOR);
	if (separatorPos != string::npos) {
		AddParam(command, "from", image_params.substr(0, separatorPos));
		AddParam(command, "to", image_params.substr(separatorPos + 1));
	}
	else {
		cerr << "Error: Invalid file descriptor '" << image_params << "', format is CURRENT_NAME:NEW_NAME" << endl;
		exit(EXIT_FAILURE);
	}

    PbResult result;
    SendCommand(hostname.c_str(), port, command, result);
}

void CommandCopyImage(PbCommand& command, const string&hostname, int port, const string& image_params)
{
	size_t separatorPos = image_params.find(COMPONENT_SEPARATOR);
	if (separatorPos != string::npos) {
		AddParam(command, "from", image_params.substr(0, separatorPos));
		AddParam(command, "to", image_params.substr(separatorPos + 1));
	}
	else {
		cerr << "Error: Invalid file descriptor '" << image_params << "', format is CURRENT_NAME:NEW_NAME" << endl;
		exit(EXIT_FAILURE);
	}

    PbResult result;
    SendCommand(hostname.c_str(), port, command, result);
}

void CommandDefaultImageFolder(PbCommand& command, const string& hostname, int port, const string& folder)
{
	AddParam(command, "folder", folder);

	PbResult result;
	SendCommand(hostname.c_str(), port, command, result);
}

void CommandDeviceInfo(const PbCommand& command, const string& hostname, int port)
{
	PbResult result;
	SendCommand(hostname.c_str(), port, command, result);

	for (const auto& pb_device : result.device_info().devices()) {
		DisplayDeviceInfo(pb_device);
	}
}

void CommandDeviceTypesInfo(const PbCommand& command, const string& hostname, int port)
{
	PbResult result;
	SendCommand(hostname.c_str(), port, command, result);

	DisplayDeviceTypesInfo(result.device_types_info());
}

void CommandServerInfo(PbCommand& command, const string& hostname, int port)
{
	PbResult result;
	SendCommand(hostname.c_str(), port, command, result);

	PbServerInfo server_info = result.server_info();

	cout << "rascsi server version: " << server_info.major_version() << "." << server_info.minor_version();
	if (server_info.patch_version() > 0) {
		cout << "." << server_info.patch_version();
	}
	else if (server_info.patch_version() < 0) {
		cout << " (development version)";
	}
	cout << endl;

	if (!server_info.log_levels_size()) {
		cout << "  No log level settings available" << endl;
	}
	else {
		cout << "rascsi log levels, sorted by severity:" << endl;
		for (const auto& log_level : server_info.log_levels()) {
			cout << "  " << log_level << endl;
		}

		cout << "Current rascsi log level: " << server_info.current_log_level() << endl;
	}

	const list<PbImageFile> image_files =
		{ server_info.image_files_info().image_files().begin(), server_info.image_files_info().image_files().end() };
	DisplayImageFiles(image_files, server_info.image_files_info().default_image_folder());

	const list<string> network_interfaces =
		{ server_info.network_interfaces_info().name().begin(), server_info.network_interfaces_info().name().end() };
	DisplayNetworkInterfaces(network_interfaces);

	DisplayDeviceTypesInfo(server_info.device_types_info());

	if (server_info.reserved_ids_size()) {
		cout << "Reserved device IDs: ";
		for (int i = 0; i < server_info.reserved_ids_size(); i++) {
			if(i) {
				cout << ", ";
			}
			cout << server_info.reserved_ids(i);
		}
		cout <<endl;
	}

	if (server_info.devices().devices_size()) {
		list<PbDevice> sorted_devices = { server_info.devices().devices().begin(), server_info.devices().devices().end() };
		sorted_devices.sort([](const auto& a, const auto& b) { return a.id() < b.id(); });

		cout << "Attached devices:" << endl;

		for (const auto& device : sorted_devices) {
			DisplayDeviceInfo(device);
		}
	}
}

void CommandImageFilesInfo(const PbCommand& command, const string& hostname, int port)
{
	PbResult result;
	SendCommand(hostname.c_str(), port, command, result);

	const list<PbImageFile> image_files =
		{ result.image_files_info().image_files().begin(),result.image_files_info().image_files().end() };
	DisplayImageFiles(image_files, result.image_files_info().default_image_folder());
}

void CommandNetworkInterfacesInfo(const PbCommand& command, const string&hostname, int port)
{
	PbResult result;
	SendCommand(hostname.c_str(), port, command, result);

	list<string> network_interfaces =
		{ result.network_interfaces_info().name().begin(), result.network_interfaces_info().name().end() };
	DisplayNetworkInterfaces(network_interfaces);
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
		// Parse convenience types (shortcuts)
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
		cerr << "Usage: " << argv[0] << " -i ID [-u UNIT] [-c CMD] [-t TYPE] [-b BLOCK_SIZE] [-n NAME] [-f FILE|PARAM] ";
		cerr << "[-d IMAGE_FOLDER] [-g LOG_LEVEL] [-h HOST] [-p PORT] [-r RESERVED_IDS] ";
		cerr << "[-a FILENAME:FILESIZE] [-w FILENAME] [-m CURRENT_NAME:NEW_NAME] [-x CURRENT_NAME:NEW_NAME] ";
		cerr << "[-e] [-k] [-l] [-v] [-y]" << endl;
		cerr << " where  ID := {0-7}" << endl;
		cerr << "        UNIT := {0|1}, default is 0" << endl;
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

	// Parse the arguments
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
	bool list = false;

	opterr = 1;
	int opt;
	while ((opt = getopt(argc, argv, "a:b:c:d:f:g:h:i:m:n:p:r:t:u:x:w:eklsvy")) != -1) {
		switch (opt) {
			case 'i':
				device->set_id(optarg[0] - '0');
				break;

			case 'u':
				device->set_unit(optarg[0] - '0');
				break;

			case 'a':
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

			case 'd':
				command.set_operation(DEFAULT_FOLDER);
				default_folder = optarg;
				break;

			case'e':
				command.set_operation(IMAGE_FILES_INFO);
				break;

			case 'f':
				param = optarg;
				break;

			case 'k':
				command.set_operation(NETWORK_INTERFACES_INFO);
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

			case 'm':
				command.set_operation(RENAME_IMAGE);
				image_params = optarg;
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
				if (!GetAsInt(optarg, port) || port <= 0 || port > 65535) {
					cerr << "Error: Invalid port " << optarg << ", port must be between 1 and 65535" << endl;
					exit(EXIT_FAILURE);
				}
				break;

			case 'r':
				command.set_operation(RESERVE);
				reserved_ids = optarg;
				break;

			case 's':
				command.set_operation(SERVER_INFO);
				break;

			case 'v':
				cout << rascsi_get_version_string() << endl;
				exit(EXIT_SUCCESS);
				break;

			case 'x':
				command.set_operation(COPY_IMAGE);
				image_params = optarg;
				break;

			case 'y':
				command.set_operation(DEVICE_TYPES_INFO);
				break;

			case 'w':
				command.set_operation(DELETE_IMAGE);
				image_params = optarg;
				break;
		}
	}

	if (optopt) {
		exit(EXIT_FAILURE);
	}

	switch(command.operation()) {
		case LOG_LEVEL:
			CommandLogLevel(command, hostname, port, log_level);
			exit(EXIT_SUCCESS);

		case DEFAULT_FOLDER:
			CommandDefaultImageFolder(command, hostname, port, default_folder);
			exit(EXIT_SUCCESS);

		case RESERVE:
			CommandReserve(command, hostname, port, reserved_ids);
			exit(EXIT_SUCCESS);

		case CREATE_IMAGE:
			CommandCreateImage(command, hostname, port, image_params);
			exit(EXIT_SUCCESS);

		case DELETE_IMAGE:
			CommandDeleteImage(command, hostname, port, image_params);
			exit(EXIT_SUCCESS);

		case RENAME_IMAGE:
			CommandRenameImage(command, hostname, port, image_params);
			exit(EXIT_SUCCESS);

		case COPY_IMAGE:
			CommandCopyImage(command, hostname, port, image_params);
			exit(EXIT_SUCCESS);

		case DEVICES_INFO:
			CommandDeviceInfo(command, hostname, port);
			exit(EXIT_SUCCESS);

		case DEVICE_TYPES_INFO:
			CommandDeviceTypesInfo(command, hostname, port);
			exit(EXIT_SUCCESS);

		case SERVER_INFO:
			CommandServerInfo(command, hostname, port);
			exit(EXIT_SUCCESS);

		case IMAGE_FILES_INFO:
			CommandImageFilesInfo(command, hostname, port);
			exit(EXIT_SUCCESS);

		case NETWORK_INTERFACES_INFO:
			CommandNetworkInterfacesInfo(command, hostname, port);
			exit(EXIT_SUCCESS);

		default:
			break;
	}

	if (list) {
		CommandList(hostname, port);
		exit(EXIT_SUCCESS);
	}

	if (!param.empty()) {
		if (device->type() == SCBR || device->type() == SCDP) {
			AddParam(*device, "interfaces", param);
		}
		else {
			AddParam(*device, "file", param);
		}
	}

	PbResult result;
	SendCommand(hostname, port, command, result);

	exit(EXIT_SUCCESS);
}

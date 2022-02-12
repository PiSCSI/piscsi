//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <netdb.h>
#include "os.h"
#include "exceptions.h"
#include "protobuf_util.h"
#include "rasutil.h"
#include "rasctl_commands.h"
#include "rascsi_interface.pb.h"
#include <sstream>
#include <iostream>
#include <list>

// Separator for the INQUIRY name components
#define COMPONENT_SEPARATOR ':'

using namespace std;
using namespace rascsi_interface;
using namespace protobuf_util;

RasctlCommands::RasctlCommands(PbCommand& command, const string& hostname, int port, const string& token,
		const string& locale)
{
	this->command = command;
	this->hostname = hostname;
	this->port = port;
	this->token = token;
	this->locale = locale;
}

void RasctlCommands::SendCommand()
{
	if (!token.empty()) {
		AddParam(command, "token", token);
	}

	if (!locale.empty()) {
		AddParam(command, "locale", locale);
	}

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

    	if (write(fd, "RASCSI", 6) != 6) {
    		throw io_exception("Can't write magic");
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

void RasctlCommands::CommandDevicesInfo()
{
	SendCommand();

	rasctl_display.DisplayDevices(result.devices_info());
}

void RasctlCommands::CommandLogLevel(const string& log_level)
{
	AddParam(command, "level", log_level);

	SendCommand();
}

void RasctlCommands::CommandReserveIds(const string& reserved_ids)
{
	AddParam(command, "ids", reserved_ids);

    SendCommand();
}

void RasctlCommands::CommandCreateImage(const string& image_params)
{
	size_t separator_pos = image_params.find(COMPONENT_SEPARATOR);
	if (separator_pos != string::npos) {
		AddParam(command, "file", image_params.substr(0, separator_pos));
		AddParam(command, "size", image_params.substr(separator_pos + 1));
	}
	else {
		cerr << "Error: Invalid file descriptor '" << image_params << "', format is NAME:SIZE" << endl;
		exit(EXIT_FAILURE);
	}

	AddParam(command, "read_only", "false");

    SendCommand();
}

void RasctlCommands::CommandDeleteImage(const string& filename)
{
	AddParam(command, "file", filename);

    SendCommand();
}

void RasctlCommands::CommandRenameImage(const string& image_params)
{
	size_t separator_pos = image_params.find(COMPONENT_SEPARATOR);
	if (separator_pos != string::npos) {
		AddParam(command, "from", image_params.substr(0, separator_pos));
		AddParam(command, "to", image_params.substr(separator_pos + 1));
	}
	else {
		cerr << "Error: Invalid file descriptor '" << image_params << "', format is CURRENT_NAME:NEW_NAME" << endl;
		exit(EXIT_FAILURE);
	}

    SendCommand();
}

void RasctlCommands::CommandCopyImage(const string& image_params)
{
	size_t separator_pos = image_params.find(COMPONENT_SEPARATOR);
	if (separator_pos != string::npos) {
		AddParam(command, "from", image_params.substr(0, separator_pos));
		AddParam(command, "to", image_params.substr(separator_pos + 1));
	}
	else {
		cerr << "Error: Invalid file descriptor '" << image_params << "', format is CURRENT_NAME:NEW_NAME" << endl;
		exit(EXIT_FAILURE);
	}

    SendCommand();
}

void RasctlCommands::CommandDefaultImageFolder(const string& folder)
{
	AddParam(command, "folder", folder);

	SendCommand();
}

void RasctlCommands::CommandDeviceInfo()
{
	SendCommand();

	for (const auto& device : result.devices_info().devices()) {
		rasctl_display.DisplayDeviceInfo(device);
	}
}

void RasctlCommands::CommandDeviceTypesInfo()
{
	SendCommand();

	rasctl_display.DisplayDeviceTypesInfo(result.device_types_info());
}

void RasctlCommands::CommandVersionInfo()
{
	SendCommand();

	rasctl_display.DisplayVersionInfo(result.version_info());
}

void RasctlCommands::CommandServerInfo()
{
	SendCommand();

	PbServerInfo server_info = result.server_info();

	rasctl_display.DisplayVersionInfo(server_info.version_info());
	rasctl_display.DisplayLogLevelInfo(server_info.log_level_info());
	rasctl_display.DisplayImageFiles(server_info.image_files_info());
	rasctl_display.DisplayMappingInfo(server_info.mapping_info());
	rasctl_display.DisplayNetworkInterfaces(server_info.network_interfaces_info());
	rasctl_display.DisplayDeviceTypesInfo(server_info.device_types_info());
	rasctl_display.DisplayReservedIdsInfo(server_info.reserved_ids_info());
	rasctl_display.DisplayOperationInfo(server_info.operation_info());

	if (server_info.devices_info().devices_size()) {
		list<PbDevice> sorted_devices = { server_info.devices_info().devices().begin(), server_info.devices_info().devices().end() };
		sorted_devices.sort([](const auto& a, const auto& b) { return a.id() < b.id(); });

		cout << "Attached devices:" << endl;

		for (const auto& device : sorted_devices) {
			rasctl_display.DisplayDeviceInfo(device);
		}
	}
}

void RasctlCommands::CommandDefaultImageFilesInfo()
{
	SendCommand();

	rasctl_display.DisplayImageFiles(result.image_files_info());
}

void RasctlCommands::CommandImageFileInfo(const string& filename)
{
	AddParam(command, "file", filename);

	SendCommand();

	rasctl_display.DisplayImageFile(result.image_file_info());
}

void RasctlCommands::CommandNetworkInterfacesInfo()
{
	SendCommand();

	rasctl_display.DisplayNetworkInterfaces(result.network_interfaces_info());
}

void RasctlCommands::CommandLogLevelInfo()
{
	SendCommand();

	rasctl_display.DisplayLogLevelInfo(result.log_level_info());
}

void RasctlCommands::CommandReservedIdsInfo()
{
	SendCommand();

	rasctl_display.DisplayReservedIdsInfo(result.reserved_ids_info());
}

void RasctlCommands::CommandMappingInfo()
{
	SendCommand();

	rasctl_display.DisplayMappingInfo(result.mapping_info());
}

void RasctlCommands::CommandOperationInfo()
{
	SendCommand();

	rasctl_display.DisplayOperationInfo(result.operation_info());
}

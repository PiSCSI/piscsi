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
#include "rasctl_display.h"
#include "rascsi_interface.pb.h"
#include <sstream>
#include <iostream>
#include <list>

// Separator for the INQUIRY name components
#define COMPONENT_SEPARATOR ':'

using namespace std;
using namespace rascsi_interface;

void RasctlCommands::SendCommand(const string& hostname, int port, const PbCommand& command, PbResult& result)
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

const PbServerInfo RasctlCommands::GetServerInfo(const PbCommand& command, const string& hostname, int port)
{
	PbResult result;
	SendCommand(hostname.c_str(), port, command, result);

	return result.server_info();
}

void RasctlCommands::CommandList(const string& hostname, int port)
{
	PbCommand command;
	command.set_operation(DEVICES_INFO);

	PbResult result;
	SendCommand(hostname.c_str(), port, command, result);

	const list<PbDevice>& devices = { result.devices_info().devices().begin(), result.devices_info().devices().end() };
	cout << ListDevices(devices) << endl;
}

void RasctlCommands::CommandLogLevel(PbCommand& command, const string& hostname, int port, const string& log_level)
{
	AddParam(command, "level", log_level);

	PbResult result;
	SendCommand(hostname.c_str(), port, command, result);
}

void RasctlCommands::CommandReserveIds(PbCommand& command, const string& hostname, int port, const string& reserved_ids)
{
	AddParam(command, "ids", reserved_ids);

    PbResult result;
    SendCommand(hostname.c_str(), port, command, result);
}

void RasctlCommands::CommandCreateImage(PbCommand& command, const string& hostname, int port, const string& image_params)
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

void RasctlCommands::CommandDeleteImage(PbCommand& command, const string& hostname, int port, const string& filename)
{
	AddParam(command, "file", filename);

    PbResult result;
    SendCommand(hostname.c_str(), port, command, result);
}

void RasctlCommands::CommandRenameImage(PbCommand& command, const string& hostname, int port, const string& image_params)
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

void RasctlCommands::CommandCopyImage(PbCommand& command, const string& hostname, int port, const string& image_params)
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

void RasctlCommands::CommandDefaultImageFolder(PbCommand& command, const string& hostname, int port, const string& folder)
{
	AddParam(command, "folder", folder);

	PbResult result;
	SendCommand(hostname.c_str(), port, command, result);
}

void RasctlCommands::CommandDeviceInfo(const PbCommand& command, const string& hostname, int port)
{
	PbResult result;
	SendCommand(hostname.c_str(), port, command, result);

	for (const auto& device : result.devices_info().devices()) {
		rasctl_display.DisplayDeviceInfo(device);
	}
}

void RasctlCommands::CommandDeviceTypesInfo(const PbCommand& command, const string& hostname, int port)
{
	PbResult result;
	SendCommand(hostname.c_str(), port, command, result);

	rasctl_display.DisplayDeviceTypesInfo(result.device_types_info());
}

void RasctlCommands::CommandVersionInfo(PbCommand& command, const string& hostname, int port)
{
	PbResult result;
	SendCommand(hostname.c_str(), port, command, result);

	rasctl_display.DisplayVersionInfo(result.version_info());
}

void RasctlCommands::CommandServerInfo(PbCommand& command, const string& hostname, int port)
{
	PbResult result;
	SendCommand(hostname.c_str(), port, command, result);

	PbServerInfo server_info = result.server_info();

	rasctl_display.DisplayVersionInfo(server_info.version_info());
	rasctl_display.DisplayLogLevelInfo(server_info.log_level_info());
	rasctl_display.DisplayImageFiles(server_info.image_files_info());
	rasctl_display.DisplayMappingInfo(server_info.mapping_info());
	rasctl_display.DisplayNetworkInterfaces(server_info.network_interfaces_info());
	rasctl_display.DisplayDeviceTypesInfo(server_info.device_types_info());
	rasctl_display.DisplayReservedIdsInfo(server_info.reserved_ids_info());

	if (server_info.devices_info().devices_size()) {
		list<PbDevice> sorted_devices = { server_info.devices_info().devices().begin(), server_info.devices_info().devices().end() };
		sorted_devices.sort([](const auto& a, const auto& b) { return a.id() < b.id(); });

		cout << "Attached devices:" << endl;

		for (const auto& device : sorted_devices) {
			rasctl_display.DisplayDeviceInfo(device);
		}
	}
}

void RasctlCommands::CommandDefaultImageFilesInfo(const PbCommand& command, const string& hostname, int port)
{
	PbResult result;
	SendCommand(hostname.c_str(), port, command, result);

	rasctl_display.DisplayImageFiles(result.image_files_info());
}

void RasctlCommands::CommandImageFileInfo(PbCommand& command, const string& hostname, int port, const string& filename)
{
	AddParam(command, "file", filename);

	PbResult result;
	SendCommand(hostname.c_str(), port, command, result);

	rasctl_display.DisplayImageFile(result.image_file_info());
}

void RasctlCommands::CommandNetworkInterfacesInfo(const PbCommand& command, const string& hostname, int port)
{
	PbResult result;
	SendCommand(hostname.c_str(), port, command, result);

	rasctl_display.DisplayNetworkInterfaces(result.network_interfaces_info());
}

void RasctlCommands::CommandLogLevelInfo(const PbCommand& command, const string& hostname, int port)
{
	PbResult result;
	SendCommand(hostname.c_str(), port, command, result);

	rasctl_display.DisplayLogLevelInfo(result.log_level_info());
}

void RasctlCommands::CommandReservedIdsInfo(const PbCommand& command, const string& hostname, int port)
{
	PbResult result;
	SendCommand(hostname.c_str(), port, command, result);

	rasctl_display.DisplayReservedIdsInfo(result.reserved_ids_info());
}

void RasctlCommands::CommandMappingInfo(const PbCommand& command, const string& hostname, int port)
{
	PbResult result;
	SendCommand(hostname.c_str(), port, command, result);

	rasctl_display.DisplayMappingInfo(result.mapping_info());
}

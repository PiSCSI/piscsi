//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "rascsi_exceptions.h"
#include "protobuf_serializer.h"
#include "protobuf_util.h"
#include "rasutil.h"
#include "rasctl_commands.h"
#include "rascsi_interface.pb.h"
#include <unistd.h>
#include <netdb.h>
#include <iostream>
#include <list>

using namespace std;
using namespace rascsi_interface;
using namespace protobuf_util;

// Separator for the INQUIRY name components
static const char COMPONENT_SEPARATOR = ':';

RasctlCommands::RasctlCommands(const PbCommand& command, const string& hostname, int port, const string& token,
		const string& locale)
	: command(command), hostname(hostname), port(port), token(token), locale(locale)
{
}

void RasctlCommands::Execute(const string& log_level, const string& default_folder, const string& reserved_ids,
		const string& image_params, const string& filename)
{
	switch(command.operation()) {
		case LOG_LEVEL:
			CommandLogLevel(log_level);
			break;

		case DEFAULT_FOLDER:
			CommandDefaultImageFolder(default_folder);
			break;

		case RESERVE_IDS:
			CommandReserveIds(reserved_ids);
			break;

		case CREATE_IMAGE:
			CommandCreateImage(image_params);
			break;

		case DELETE_IMAGE:
			CommandDeleteImage(image_params);
			break;

		case RENAME_IMAGE:
			CommandRenameImage(image_params);
			break;

		case COPY_IMAGE:
			CommandCopyImage(image_params);
			break;

		case DEVICES_INFO:
			CommandDeviceInfo();
			break;

		case DEVICE_TYPES_INFO:
			CommandDeviceTypesInfo();
			break;

		case VERSION_INFO:
			CommandVersionInfo();
			break;

		case SERVER_INFO:
			CommandServerInfo();
			break;

		case DEFAULT_IMAGE_FILES_INFO:
			CommandDefaultImageFilesInfo();
			break;

		case IMAGE_FILE_INFO:
			CommandImageFileInfo(filename);
			break;

		case NETWORK_INTERFACES_INFO:
			CommandNetworkInterfacesInfo();
			break;

		case LOG_LEVEL_INFO:
			CommandLogLevelInfo();
			break;

		case RESERVED_IDS_INFO:
			CommandReservedIdsInfo();
			break;

		case MAPPING_INFO:
			CommandMappingInfo();
			break;

		case OPERATION_INFO:
			CommandOperationInfo();
			break;

		default:
			SendCommand();
			break;
	}
}

void RasctlCommands::SendCommand()
{
	AddParam(command, "token", token);
	AddParam(command, "locale", locale);

	int fd = -1;
	try {
		fd = socket(AF_INET, SOCK_STREAM, 0);
		if (fd == -1) {
			throw io_exception("Can't create socket: " + string(strerror(errno)));
		}

		sockaddr_in server_addr = {};
		if (!ResolveHostName(hostname, &server_addr)) {
    		throw io_exception("Can't resolve hostname '" + hostname + "'");
    	}

		server_addr.sin_port = htons(uint16_t(port));
    	if (connect(fd, (sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    		throw io_exception("Can't connect to rascsi on host '" + hostname + "', port " + to_string(port)
    				+ ": " + strerror(errno));
    	}

    	if (write(fd, "RASCSI", 6) != 6) {
    		throw io_exception("Can't write magic");
    	}

    	serializer.SerializeMessage(fd, command);
    }
    catch(const io_exception& e) {
    	cerr << "Error: " << e.get_msg() << endl;

        if (fd != -1) {
        	close(fd);
        }

        exit(fd == -1 ? ENOTCONN : EXIT_FAILURE);
    }

    // Receive result
    try {
    	serializer.DeserializeMessage(fd, result);

    	if (!result.status()) {
    		throw io_exception(result.msg());
    	}
    }
    catch(const io_exception& e) {
    	close(fd);

    	cerr << "Error: " << e.get_msg() << endl;

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

	cout << rasctl_display.DisplayDevicesInfo(result.devices_info()) << flush;
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
	if (const size_t separator_pos = image_params.find(COMPONENT_SEPARATOR); separator_pos != string::npos) {
		AddParam(command, "file", string_view(image_params).substr(0, separator_pos));
		AddParam(command, "size", string_view(image_params).substr(separator_pos + 1));
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
	if (const size_t separator_pos = image_params.find(COMPONENT_SEPARATOR); separator_pos != string::npos) {
		AddParam(command, "from", string_view(image_params).substr(0, separator_pos));
		AddParam(command, "to", string_view(image_params).substr(separator_pos + 1));
	}
	else {
		cerr << "Error: Invalid file descriptor '" << image_params << "', format is CURRENT_NAME:NEW_NAME" << endl;
		exit(EXIT_FAILURE);
	}

    SendCommand();
}

void RasctlCommands::CommandCopyImage(const string& image_params)
{
	if (const size_t separator_pos = image_params.find(COMPONENT_SEPARATOR); separator_pos != string::npos) {
		AddParam(command, "from", string_view(image_params).substr(0, separator_pos));
		AddParam(command, "to", string_view(image_params).substr(separator_pos + 1));
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
		cout << rasctl_display.DisplayDeviceInfo(device);
	}

	cout << flush;
}

void RasctlCommands::CommandDeviceTypesInfo()
{
	SendCommand();

	cout << rasctl_display.DisplayDeviceTypesInfo(result.device_types_info()) << flush;
}

void RasctlCommands::CommandVersionInfo()
{
	SendCommand();

	cout << rasctl_display.DisplayVersionInfo(result.version_info()) << flush;
}

void RasctlCommands::CommandServerInfo()
{
	SendCommand();

	PbServerInfo server_info = result.server_info();

	cout << rasctl_display.DisplayVersionInfo(server_info.version_info());
	cout << rasctl_display.DisplayLogLevelInfo(server_info.log_level_info());
	cout << rasctl_display.DisplayImageFilesInfo(server_info.image_files_info());
	cout << rasctl_display.DisplayMappingInfo(server_info.mapping_info());
	cout << rasctl_display.DisplayNetworkInterfaces(server_info.network_interfaces_info());
	cout << rasctl_display.DisplayDeviceTypesInfo(server_info.device_types_info());
	cout << rasctl_display.DisplayReservedIdsInfo(server_info.reserved_ids_info());
	cout << rasctl_display.DisplayOperationInfo(server_info.operation_info());

	if (server_info.devices_info().devices_size()) {
		list<PbDevice> sorted_devices = { server_info.devices_info().devices().begin(), server_info.devices_info().devices().end() };
		sorted_devices.sort([](const auto& a, const auto& b) { return a.id() < b.id() || a.unit() < b.unit(); });

		cout << "Attached devices:\n";

		for (const auto& device : sorted_devices) {
			cout << rasctl_display.DisplayDeviceInfo(device);
		}
	}

	cout << flush;
}

void RasctlCommands::CommandDefaultImageFilesInfo()
{
	SendCommand();

	cout << rasctl_display.DisplayImageFilesInfo(result.image_files_info()) << flush;
}

void RasctlCommands::CommandImageFileInfo(const string& filename)
{
	AddParam(command, "file", filename);

	SendCommand();

	cout << rasctl_display.DisplayImageFile(result.image_file_info()) << flush;
}

void RasctlCommands::CommandNetworkInterfacesInfo()
{
	SendCommand();

	cout << rasctl_display.DisplayNetworkInterfaces(result.network_interfaces_info()) << flush;
}

void RasctlCommands::CommandLogLevelInfo()
{
	SendCommand();

	cout << rasctl_display.DisplayLogLevelInfo(result.log_level_info()) << flush;
}

void RasctlCommands::CommandReservedIdsInfo()
{
	SendCommand();

	cout << rasctl_display.DisplayReservedIdsInfo(result.reserved_ids_info()) << flush;
}

void RasctlCommands::CommandMappingInfo()
{
	SendCommand();

	cout << rasctl_display.DisplayMappingInfo(result.mapping_info()) << flush;
}

void RasctlCommands::CommandOperationInfo()
{
	SendCommand();

	cout << rasctl_display.DisplayOperationInfo(result.operation_info()) << flush;
}

bool RasctlCommands::ResolveHostName(const string& host, sockaddr_in *addr)
{
	addrinfo hints = {};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	if (addrinfo *result; !getaddrinfo(host.c_str(), nullptr, &hints, &result)) {
		*addr = *(sockaddr_in *)(result->ai_addr);
		freeaddrinfo(result);
		return true;
	}

	return false;
}

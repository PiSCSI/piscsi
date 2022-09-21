//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "socket_connector.h"
#include "rascsi_interface.pb.h"
#include "rasctl_display.h"
#include <string>

using namespace std;
using namespace rascsi_interface;

class RasctlCommands
{
public:

	RasctlCommands(const PbCommand&, const string&, int, const string&, const string&);
	~RasctlCommands() = default;
	RasctlCommands(RasctlCommands&) = delete;
	RasctlCommands& operator=(const RasctlCommands&) = delete;

	void SendCommand();
	void CommandDevicesInfo();
	void CommandLogLevel(const string&);
	void CommandReserveIds(const string&);
	void CommandCreateImage(const string&);
	void CommandDeleteImage(const string&);
	void CommandRenameImage(const string&);
	void CommandCopyImage(const string&);
	void CommandDefaultImageFolder(const string&);
	void CommandDeviceInfo();
	void CommandDeviceTypesInfo();
	void CommandVersionInfo();
	void CommandServerInfo();
	void CommandDefaultImageFilesInfo();
	void CommandImageFileInfo(const string&);
	void CommandNetworkInterfacesInfo();
	void CommandLogLevelInfo();
	void CommandReservedIdsInfo();
	void CommandMappingInfo();
	void CommandOperationInfo();

private:

	SocketConnector socket_connector;
	PbCommand command;
	string hostname;
	int port;
	string token;
	string locale;

	PbResult result;

	RasctlDisplay rasctl_display;
};

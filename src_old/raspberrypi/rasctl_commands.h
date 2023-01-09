//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "rascsi_interface.pb.h"
#include "rasctl_display.h"
#include <string>

using namespace std;
using namespace rascsi_interface;

class RasctlCommands
{
public:

	RasctlCommands(PbCommand&, const string&, int, const string&, const string&);
	~RasctlCommands() {};

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

	PbCommand command;
	string hostname;
	int port;
	string token;
	string locale;

	PbResult result;

	RasctlDisplay rasctl_display;
};

//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "shared/protobuf_serializer.h"
#include "generated/piscsi_interface.pb.h"
#include "scsictl_display.h"
#include <string>

using namespace piscsi_interface;

struct sockaddr_in;

class RasctlCommands
{
public:

	RasctlCommands(PbCommand& command, const string& hostname, int port)
		: command(command), hostname(hostname), port(port) {}
	~RasctlCommands() = default;

	bool Execute(const string&, const string&, const string&, const string&, const string&);

	bool CommandDevicesInfo();

private:

	bool CommandLogLevel(const string&);
	bool CommandReserveIds(const string&);
	bool CommandCreateImage(const string&);
	bool CommandDeleteImage(const string&);
	bool CommandRenameImage(const string&);
	bool CommandCopyImage(const string&);
	bool CommandDefaultImageFolder(const string&);
	bool CommandDeviceInfo();
	bool CommandDeviceTypesInfo();
	bool CommandVersionInfo();
	bool CommandServerInfo();
	bool CommandDefaultImageFilesInfo();
	bool CommandImageFileInfo(const string&);
	bool CommandNetworkInterfacesInfo();
	bool CommandLogLevelInfo();
	bool CommandReservedIdsInfo();
	bool CommandMappingInfo();
	bool CommandOperationInfo();
	bool SendCommand();

	static bool ResolveHostName(const string&, sockaddr_in *);

	ProtobufSerializer serializer;
	PbCommand& command;
	string hostname;
	int port;

	PbResult result;

	RasctlDisplay scsictl_display;
};

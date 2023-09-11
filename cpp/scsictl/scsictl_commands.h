//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2021-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "shared/protobuf_serializer.h"
#include "generated/piscsi_interface.pb.h"
#include "scsictl_display.h"
#include <string>

using namespace piscsi_interface;

struct sockaddr_in;

class ScsictlCommands
{
public:

	ScsictlCommands(PbCommand& command, const string& hostname, int port)
		: command(command), hostname(hostname), port(port) {}
	~ScsictlCommands() = default;

	bool Execute(string_view, string_view, string_view, string_view, string_view);

	bool CommandDevicesInfo();

private:

	bool CommandLogLevel(string_view);
	bool CommandReserveIds(string_view);
	bool CommandCreateImage(string_view);
	bool CommandDeleteImage(string_view);
	bool CommandRenameImage(string_view);
	bool CommandCopyImage(string_view);
	bool CommandDefaultImageFolder(string_view);
	bool CommandDeviceInfo();
	bool CommandDeviceTypesInfo();
	bool CommandVersionInfo();
	bool CommandServerInfo();
	bool CommandDefaultImageFilesInfo();
	bool CommandImageFileInfo(string_view);
	bool CommandNetworkInterfacesInfo();
	bool CommandLogLevelInfo();
	bool CommandReservedIdsInfo();
	bool CommandMappingInfo();
	bool CommandOperationInfo();
	bool SendCommand();
	bool EvaluateParams(string_view, const string&, const string&);

	static bool ResolveHostName(const string&, sockaddr_in *);

	[[no_unique_address]] const ProtobufSerializer serializer;
	PbCommand& command;
	string hostname;
	int port;

	PbResult result;

	[[no_unique_address]] const ScsictlDisplay scsictl_display;
};

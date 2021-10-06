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

	RasctlCommands() {};
	~RasctlCommands() {};

	void SendCommand(const string&, int, const PbCommand&, PbResult&);
	const PbServerInfo GetServerInfo(const PbCommand&, const string&, int);
	void CommandList(const string&, int);
	void CommandLogLevel(PbCommand&, const string&, int, const string&);
	void CommandReserveIds(PbCommand&, const string&, int, const string&);
	void CommandCreateImage(PbCommand&, const string&, int, const string&);
	void CommandDeleteImage(PbCommand&, const string&, int, const string&);
	void CommandRenameImage(PbCommand&, const string&, int, const string&);
	void CommandCopyImage(PbCommand&, const string&, int, const string&);
	void CommandDefaultImageFolder(PbCommand&, const string&, int, const string&);
	void CommandDeviceInfo(const PbCommand&, const string&, int);
	void CommandDeviceTypesInfo(const PbCommand&, const string&, int);
	void CommandVersionInfo(PbCommand&, const string&, int);
	void CommandServerInfo(PbCommand&, const string&, int);
	void CommandDefaultImageFilesInfo(const PbCommand&, const string&, int);
	void CommandImageFileInfo(PbCommand&, const string&, int, const string&);
	void CommandNetworkInterfacesInfo(const PbCommand&, const string&, int);
	void CommandLogLevelInfo(const PbCommand&, const string&, int);
	void CommandReservedIdsInfo(const PbCommand&, const string&, int);
	void CommandMappingInfo(const PbCommand&, const string&, int);

private:

	RasctlDisplay rasctl_display;
};

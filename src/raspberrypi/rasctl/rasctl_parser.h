//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "rascsi_interface.pb.h"
#include <string>
#include <unordered_map>

using namespace std;
using namespace rascsi_interface;

class RasctlParser
{
public:

	RasctlParser() = default;
	~RasctlParser() = default;

	PbOperation ParseOperation(const string&) const;
	PbDeviceType ParseType(const string&) const;

private:

	unordered_map<char, PbOperation> operations = {
			{ 'a', ATTACH },
			{ 'd', DETACH },
			{ 'i', INSERT },
			{ 'e', EJECT },
			{ 'p', PROTECT },
			{ 'u', UNPROTECT }
	};

	unordered_map<char, PbDeviceType> device_types = {
			{ 'b', SCBR },
			{ 'c', SCCD },
			{ 'd', SCDP },
			{ 'h', SCHD },
			{ 'l', SCLP },
			{ 'm', SCMO },
			{ 'r', SCRM },
			{ 's', SCHS }
	};
};

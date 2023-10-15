//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "generated/piscsi_interface.pb.h"
#include <string>
#include <unordered_map>

using namespace std;
using namespace piscsi_interface;

class ScsictlParser
{

public:

	ScsictlParser() = default;
	~ScsictlParser() = default;

	PbOperation ParseOperation(string_view) const;
	PbDeviceType ParseType(const string&) const;

private:

	const unordered_map<int, PbOperation> operations = {
			{ 'a', ATTACH },
			{ 'd', DETACH },
			{ 'e', EJECT },
			{ 'i', INSERT },
			{ 'p', PROTECT },
			{ 's', DEVICES_INFO },
			{ 'u', UNPROTECT }
	};

	const unordered_map<int, PbDeviceType> device_types = {
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

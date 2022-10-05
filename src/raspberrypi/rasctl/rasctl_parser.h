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

using namespace std;
using namespace rascsi_interface;

class RasctlParser
{
public:

	RasctlParser() = default;
	~RasctlParser() = default;

	PbOperation ParseOperation(const string&) const;
	PbDeviceType ParseType(const char *) const;
};

//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
// Helper methods for setting up/evaluating protobuf messages
//
//---------------------------------------------------------------------------

#pragma once

#include "google/protobuf/message.h"
#include "rascsi_interface.pb.h"
#include <string>

using namespace std;
using namespace rascsi_interface;

namespace command_util
{
	void ParseParameters(PbDeviceDefinition&, const string&);
	string GetParam(const PbCommand&, const string&);
	string GetParam(const PbDeviceDefinition&, const string&);
	void AddParam(PbCommand&, const string&, string_view);
	void AddParam(PbDevice&, const string&, string_view);
	void AddParam(PbDeviceDefinition&, const string&, string_view);
}

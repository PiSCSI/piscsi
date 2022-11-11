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
#include "generated/rascsi_interface.pb.h"
#include <string>
#include <list>

using namespace std;
using namespace rascsi_interface;

namespace protobuf_util
{
	static const char KEY_VALUE_SEPARATOR = '=';

	void ParseParameters(PbDeviceDefinition&, const string&);
	string GetParam(const PbCommand&, const string&);
	string GetParam(const PbDeviceDefinition&, const string&);
	void SetParam(PbCommand&, const string&, string_view);
	void SetParam(PbDevice&, const string&, string_view);
	void SetParam(PbDeviceDefinition&, const string&, string_view);
	void SetPatternParams(PbCommand&, string_view);
	void SetProductData(PbDeviceDefinition&, const string&);
	string SetIdAndLun(PbDeviceDefinition&, const string&, int);
	string ListDevices(const list<PbDevice>&);
}

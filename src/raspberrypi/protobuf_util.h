//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
// Helper methods for serializing/deserializing protobuf messages
//
//---------------------------------------------------------------------------

#pragma once

#include "google/protobuf/message.h"
#include "rascsi_interface.pb.h"
#include "command_context.h"
#include "localizer.h"
#include <sstream>
#include <string>

using namespace std;
using namespace rascsi_interface;

namespace protobuf_util
{
	void ParseParameters(PbDeviceDefinition&, const string&);
	string GetParam(const PbCommand&, const string&);
	string GetParam(const PbDeviceDefinition&, const string&);
	void AddParam(PbCommand&, const string&, const string&);
	void AddParam(PbDevice&, const string&, const string&);
	void AddParam(PbDeviceDefinition&, const string&, const string&);
	void SerializeMessage(int, const google::protobuf::Message&);
	void DeserializeMessage(int, google::protobuf::Message&);
	size_t ReadNBytes(int, uint8_t *, size_t);
	bool ReturnLocalizedError(const CommandContext&, const LocalizationKey, const string& = "", const string& = "",
			const string& = "");
	bool ReturnLocalizedError(const CommandContext&, const LocalizationKey, const PbErrorCode, const string& = "",
			const string& = "", const string& = "");
	bool ReturnStatus(const CommandContext&, bool = true, const string& = "",
			const PbErrorCode = PbErrorCode::NO_ERROR_CODE, bool = true);
}

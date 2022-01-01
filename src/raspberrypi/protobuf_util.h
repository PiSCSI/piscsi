//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
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
	const string GetParam(const PbCommand&, const string&);
	const string GetParam(const PbDeviceDefinition&, const string&);
	void AddParam(PbCommand&, const string&, const string&);
	void AddParam(PbDevice&, const string&, const string&);
	void AddParam(PbDeviceDefinition&, const string&, const string&);
	void SerializeMessage(int, const google::protobuf::Message&);
	void DeserializeMessage(int, google::protobuf::Message&);
	int ReadNBytes(int, uint8_t *, int);
	bool ReturnLocalizedError(const CommandContext&, const LocalizationKey, const string& = "", const string& = "",
			const string& = "");
	bool ReturnLocalizedError(const CommandContext&, const LocalizationKey, const PbErrorCode, const string& = "",
			const string& = "", const string& = "");
	bool ReturnStatus(const CommandContext&, bool = true, const string& = "",
			const PbErrorCode = PbErrorCode::NO_ERROR_CODE, bool = true);
}

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
#include <string>

using namespace rascsi_interface; //NOSONAR Not relevant for rascsi

namespace command_util
{
	void ParseParameters(PbDeviceDefinition&, const string&);
	string GetParam(const PbCommand&, const string&);
	string GetParam(const PbDeviceDefinition&, const string&);
	void AddParam(PbCommand&, const string&, string_view);
	void AddParam(PbDevice&, const string&, string_view);
	void AddParam(PbDeviceDefinition&, const string&, string_view);
	bool ReturnLocalizedError(const CommandContext&, const LocalizationKey, const string& = "", const string& = "",
			const string& = "");
	bool ReturnLocalizedError(const CommandContext&, const LocalizationKey, const PbErrorCode, const string& = "",
			const string& = "", const string& = "");
	bool ReturnStatus(const CommandContext&, bool = true, const string& = "",
			const PbErrorCode = PbErrorCode::NO_ERROR_CODE, bool = true);
}

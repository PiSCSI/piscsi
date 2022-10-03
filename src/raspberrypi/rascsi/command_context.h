//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "rascsi_interface.pb.h"
#include "localizer.h"
#include "protobuf_serializer.h"
#include <string>

using namespace rascsi_interface;

class CommandContext
{
public:

	CommandContext(int f, const std::string& s) : fd(f), locale(s) {}
	~CommandContext() = default;

	const ProtobufSerializer serializer;
	const Localizer localizer;
	int fd;
	std::string locale;

	bool ReturnLocalizedError(LocalizationKey, const string& = "", const string& = "", const string& = "") const;
	bool ReturnLocalizedError(LocalizationKey, PbErrorCode, const string& = "", const string& = "", const string& = "") const;
	bool ReturnStatus(bool = true, const string& = "", PbErrorCode = PbErrorCode::NO_ERROR_CODE, bool = true) const;
};

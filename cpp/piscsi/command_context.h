//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "generated/piscsi_interface.pb.h"
#include "localizer.h"
#include "shared/protobuf_serializer.h"
#include <string>

using namespace std;
using namespace piscsi_interface;

class CommandContext
{
	const ProtobufSerializer serializer;

	const Localizer localizer;

	string locale;

	int fd;

public:

	CommandContext(const std::string& s = "", int f = -1) : locale(s), fd(f) {}
	~CommandContext() = default;

	void Cleanup();

	const ProtobufSerializer& GetSerializer() const { return serializer; }
	int GetFd() const { return fd; }
	void SetFd(int f) { fd = f; }
	bool IsValid() const { return fd != -1; }

	bool ReturnLocalizedError(LocalizationKey, const string& = "", const string& = "", const string& = "") const;
	bool ReturnLocalizedError(LocalizationKey, PbErrorCode, const string& = "", const string& = "", const string& = "") const;
	bool ReturnStatus(bool = true, const string& = "", PbErrorCode = PbErrorCode::NO_ERROR_CODE, bool = true) const;
};

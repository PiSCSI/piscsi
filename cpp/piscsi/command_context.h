//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2021-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "localizer.h"
#include "shared/protobuf_serializer.h"
#include "generated/piscsi_interface.pb.h"
#include <string>

using namespace std;
using namespace piscsi_interface;

class CommandContext
{
	const ProtobufSerializer serializer;

	const Localizer localizer;

	PbCommand command;

	string locale;

	int fd = -1;

public:

	explicit CommandContext(const string& s = "") : locale(s) {}
	~CommandContext() = default;

	bool ReadCommand(int);
	const PbCommand& GetCommand() const { return command; }
	void WriteResult(const google::protobuf::Message& msg) const { serializer.SerializeMessage(fd, msg); }

	// TODO Try to get rid of this method
	bool IsValid() const { return fd != -1; }

	bool ReturnLocalizedError(LocalizationKey, const string& = "", const string& = "", const string& = "") const;
	bool ReturnLocalizedError(LocalizationKey, PbErrorCode, const string& = "", const string& = "", const string& = "") const;
	bool ReturnSuccessStatus() const;
	bool ReturnErrorStatus(const string&) const;

private:

	bool ReturnStatus(bool, const string&, PbErrorCode, bool) const;
};

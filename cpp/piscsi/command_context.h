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

	CommandContext(const PbCommand& cmd, const string& s = "") : command(cmd), locale(s) {}
	explicit CommandContext(int f) : fd(f) {}
	~CommandContext() = default;

	bool ReadCommand();
	void WriteResult(const PbResult&) const;
	const PbCommand& GetCommand() const { return command; }

	// TODO Try to get rid of this method
	bool IsValid() const { return fd != -1; }

	bool ReturnLocalizedError(LocalizationKey, const string& = "", const string& = "", const string& = "") const;
	bool ReturnLocalizedError(LocalizationKey, PbErrorCode, const string& = "", const string& = "", const string& = "") const;
	bool ReturnSuccessStatus() const;
	bool ReturnErrorStatus(const string&) const;

private:

	bool ReturnStatus(bool, const string&, PbErrorCode, bool) const;
};

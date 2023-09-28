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
#include "generated/piscsi_interface.pb.h"
#include <string>

using namespace std;
using namespace piscsi_interface;

class CommandContext
{
	const Localizer localizer;

	PbCommand command;

	const string locale;

	int fd = -1;

public:

	CommandContext(const PbCommand& cmd, const string& f, const string& l)
		: command(cmd), locale(l), default_folder(f) {}
	explicit CommandContext(int f) : fd(f) {}
	~CommandContext() = default;

	string GetDefaultFolder() const { return default_folder; }
	void SetDefaultFolder(const string& f) { default_folder = f; }
	bool ReadCommand();
	void WriteResult(const PbResult&) const;
	const PbCommand& GetCommand() const { return command; }

	bool ReturnLocalizedError(LocalizationKey, const string& = "", const string& = "", const string& = "") const;
	bool ReturnLocalizedError(LocalizationKey, PbErrorCode, const string& = "", const string& = "", const string& = "") const;
	bool ReturnSuccessStatus() const;
	bool ReturnErrorStatus(const string&) const;

private:

	bool ReturnStatus(bool, const string&, PbErrorCode, bool) const;

	string default_folder;
};

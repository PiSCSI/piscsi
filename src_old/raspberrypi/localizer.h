//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
// Message localization support. Currently only for messages with up to 3 string parameters.
//
//---------------------------------------------------------------------------

#pragma once

#include <string>
#include <set>
#include <map>

using namespace std;

enum LocalizationKey {
	ERROR_AUTHENTICATION,
	ERROR_OPERATION,
	ERROR_LOG_LEVEL,
	ERROR_MISSING_DEVICE_ID,
	ERROR_MISSING_FILENAME,
	ERROR_IMAGE_IN_USE,
	ERROR_RESERVED_ID,
	ERROR_NON_EXISTING_DEVICE,
	ERROR_NON_EXISTING_UNIT,
	ERROR_UNKNOWN_DEVICE_TYPE,
	ERROR_MISSING_DEVICE_TYPE,
	ERROR_DUPLICATE_ID,
	ERROR_SASI_SCSI,
	ERROR_EJECT_REQUIRED,
	ERROR_DEVICE_NAME_UPDATE,
	ERROR_SHUTDOWN_MODE_MISSING,
	ERROR_SHUTDOWN_MODE_INVALID,
	ERROR_SHUTDOWN_PERMISSION,
	ERROR_FILE_OPEN,
	ERROR_BLOCK_SIZE,
	ERROR_BLOCK_SIZE_NOT_CONFIGURABLE
};

class Localizer
{
public:

	Localizer();
	~Localizer() {};

	string Localize(LocalizationKey, const string&, const string& = "", const string& = "", const string& = "");

private:

	void Add(LocalizationKey, const string&, const string&);
	map<string, map<LocalizationKey, string>> localized_messages;

	set<string> supported_languages;
};

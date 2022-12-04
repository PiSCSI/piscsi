//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
// Message localization support. Currently only for messages with up to 3 string parameters.
//
//---------------------------------------------------------------------------

#pragma once

#include <string>
#include <unordered_set>
#include <unordered_map>

using namespace std;

enum class LocalizationKey {
	ERROR_AUTHENTICATION,
	ERROR_OPERATION,
	ERROR_LOG_LEVEL,
	ERROR_MISSING_DEVICE_ID,
	ERROR_MISSING_FILENAME,
	ERROR_DEVICE_MISSING_FILENAME,
	ERROR_IMAGE_IN_USE,
	ERROR_IMAGE_FILE_INFO,
	ERROR_RESERVED_ID,
	ERROR_NON_EXISTING_DEVICE,
	ERROR_NON_EXISTING_UNIT,
	ERROR_UNKNOWN_DEVICE_TYPE,
	ERROR_MISSING_DEVICE_TYPE,
	ERROR_DUPLICATE_ID,
	ERROR_DETACH,
	ERROR_EJECT_REQUIRED,
	ERROR_DEVICE_NAME_UPDATE,
	ERROR_SHUTDOWN_MODE_MISSING,
	ERROR_SHUTDOWN_MODE_INVALID,
	ERROR_SHUTDOWN_PERMISSION,
	ERROR_FILE_OPEN,
	ERROR_BLOCK_SIZE,
	ERROR_BLOCK_SIZE_NOT_CONFIGURABLE,
	ERROR_SCSI_CONTROLLER,
	ERROR_INVALID_ID,
	ERROR_INVALID_LUN,
	ERROR_LUN0,
	ERROR_INITIALIZATION,
	ERROR_OPERATION_DENIED_STOPPABLE,
	ERROR_OPERATION_DENIED_REMOVABLE,
	ERROR_OPERATION_DENIED_PROTECTABLE,
	ERROR_OPERATION_DENIED_READY
};

class Localizer
{
public:

	Localizer();
	~Localizer() = default;

	string Localize(LocalizationKey, const string&, const string& = "", const string& = "", const string& = "") const;

private:

	void Add(LocalizationKey, const string&, string_view);
	unordered_map<string, unordered_map<LocalizationKey, string>> localized_messages;

	// Supported locales, always lower case
	unordered_set<string> supported_languages = { "en", "de", "sv", "fr", "es" };
};

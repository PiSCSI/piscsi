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
	ERROR_MISSING_FILENAME,
	ERROR_UNKNOWN_DEVICE_TYPE,
	ERROR_MISSING_DEVICE_TYPE,
	ERROR_DUPLICATE_ID
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

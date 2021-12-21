//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "localizer.h"
#include <cassert>
#include <string>
#include <map>
#include <iostream>

using namespace std;

Localizer::Localizer()
{
	Add("en", "ERROR_AUTHENTICATION", "Authentication failed");
	Add("de", "ERROR_AUTHENTICATION", "Authentifizierung fehlgeschlagen");
}

void Localizer::Add(const string& locale, const string& key, const string& value)
{
	localized_messages[locale][key] = value;
}

string Localizer::Localize(const string& locale, const string& key)
{
	map<string, string> messages = localized_messages[locale];
	if (messages.empty()) {
		messages = localized_messages["en"];
	}
	assert(!messages.empty());

	string message = messages[key];
	if (messages.empty()) {
		return "Missing localization for key '" + key + "'";
	}

	return message;
}

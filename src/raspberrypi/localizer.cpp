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
#include <sstream>
#include <iostream>

using namespace std;

Localizer::Localizer()
{
	// Supported locales
	supported_languages = { "en", "de" };

	Add(ERROR_AUTHENTICATION, "en", "Authentication failed");
	Add(ERROR_AUTHENTICATION, "de", "Authentifizierung fehlgeschlagen");

	assert(localized_messages.size() == KEY_COUNT * supported_languages.size());
}

void Localizer::Add(LocalizationKey key, const string& locale, const string& value)
{
	assert(supported_languages.find(locale) != supported_languages.end());

	localized_messages[locale][key] = value;
}

string Localizer::Localize(LocalizationKey key, const string& locale)
{
	map<LocalizationKey, string> messages = localized_messages[locale];
	if (messages.empty()) {
		messages = localized_messages["en"];
	}

	assert(!messages.empty());

	string message = messages[key];
	if (messages.empty()) {
		stringstream s;
		s << "Missing localization for enum value " << key;
		return s.str();
	}

	return message;
}

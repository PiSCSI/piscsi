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
	Add(ERROR_OPERATION, "en", "Unknown operation");
	Add(ERROR_OPERATION, "de", "Unbekannte Operation");

}

void Localizer::Add(LocalizationKey key, const string& locale, const string& value)
{
	// Safeguards against duplicate entries and unsupported locales
	assert(supported_languages.find(locale) != supported_languages.end());
	assert(localized_messages[locale][key].empty());

	localized_messages[locale][key] = value;
}

string Localizer::Localize(LocalizationKey key, const string& locale)
{
	map<LocalizationKey, string> messages = localized_messages[locale];
	if (messages.empty()) {
		// Try to fall back to country-indepedent locale (e.g. "en" instead of "en_US")
		if (locale.length() > 2) {
			messages = localized_messages[locale.substr(0, 2)];
		}
		if (messages.empty()) {
			messages = localized_messages["en"];
		}
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

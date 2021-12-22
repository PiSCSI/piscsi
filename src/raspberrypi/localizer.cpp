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
#include <algorithm>
#include <regex>

using namespace std;

Localizer::Localizer()
{
	// Supported locales, always lower case
	supported_languages = { "en", "de" };

	// Positional string arguments are %1, %2, %3
	Add(ERROR_AUTHENTICATION, "en", "Authentication failed");
	Add(ERROR_AUTHENTICATION, "de", "Authentifizierung fehlgeschlagen");
	Add(ERROR_OPERATION, "en", "Unknown operation");
	Add(ERROR_OPERATION, "de", "Unbekannte Operation");
	Add(ERROR_LOG_LEVEL, "en", "Invalid log level %1");
	Add(ERROR_LOG_LEVEL, "de", "Ungültiger Log-Level %1");
	Add(ERROR_MISSING_FILENAME, "en", "Missing filename");
	Add(ERROR_MISSING_FILENAME, "de", "Fehlender Dateiname");
	Add(ERROR_UNKNOWN_DEVICE_TYPE, "en", "Unknown device type %1");
	Add(ERROR_UNKNOWN_DEVICE_TYPE, "de", "Unbekannter Gerätetyp %1");
	Add(ERROR_MISSING_DEVICE_TYPE, "en", "Device type required for unknown extension of file '%1'");
	Add(ERROR_MISSING_DEVICE_TYPE, "de", "Gerätetyp erforderlich für unbekannte Extension der Datei '%1'");
	Add(ERROR_DUPLICATE_ID, "en", "Duplicate ID %1, unit %2");
	Add(ERROR_DUPLICATE_ID, "de", "Doppelte ID %1, Einheit %2");
}

void Localizer::Add(LocalizationKey key, const string& locale, const string& value)
{
	// Safeguards against empty messages, duplicate entries and unsupported locales
	assert(locale.size());
	assert(value.size());
	assert(supported_languages.find(locale) != supported_languages.end());
	assert(localized_messages[locale][key].empty());

	localized_messages[locale][key] = value;
}

string Localizer::Localize(LocalizationKey key, const string& locale, const string& arg1, const string& arg2,
		const string &arg3)
{
	string locale_lower = locale;
	transform(locale_lower.begin(), locale_lower.end(), locale_lower.begin(), ::tolower);

	map<LocalizationKey, string> messages = localized_messages[locale_lower];
	if (messages.empty()) {
		// Try to fall back to country-indepedent locale (e.g. "en" instead of "en_US")
		if (locale_lower.length() > 2) {
			messages = localized_messages[locale_lower.substr(0, 2)];
		}
		if (messages.empty()) {
			messages = localized_messages["en"];
		}
	}

	assert(!messages.empty());

	string message = messages[key];
	if (messages.empty()) {
		return "Missing localization for enum value " + to_string(key);
	}

	message = regex_replace(message, regex("%1"), arg1);
	message = regex_replace(message, regex("%2"), arg2);
	message = regex_replace(message, regex("%3"), arg3);

	return message;
}

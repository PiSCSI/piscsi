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
	Add(ERROR_AUTHENTICATION, "en", "Authentication failed");
	Add(ERROR_AUTHENTICATION, "de", "Authentifizierung fehlgeschlagen");
}

void Localizer::Add(LocalizationKey key, const string& locale, const string& value)
{
	localized_messages[key][locale] = value;
}

string Localizer::Localize(LocalizationKey key, const string& locale)
{
	map<string, string> messages = localized_messages[key];
	if (messages.empty()) {
		stringstream s;
		s << "Missing localization for enum value " << key;
		return s.str();
	}

	return messages[locale];
}

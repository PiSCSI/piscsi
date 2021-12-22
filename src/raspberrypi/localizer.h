//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <string>
#include <set>
#include <map>

using namespace std;

enum LocalizationKey {
	ERROR_AUTHENTICATION,
	// The value below is used for a consistency check
	KEY_COUNT
};

class Localizer
{
public:

	Localizer();
	~Localizer() {};

	string Localize(LocalizationKey, const string&);

private:

	void Add(LocalizationKey, const string&, const string&);
	map<string, map<LocalizationKey, string>> localized_messages;

	set<string> supported_languages;
};

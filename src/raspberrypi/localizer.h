//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
// Message localization support. Currently for non parameterized messages only.
//
//---------------------------------------------------------------------------

#pragma once

#include <string>
#include <set>
#include <map>

using namespace std;

enum LocalizationKey {
	ERROR_AUTHENTICATION,
	ERROR_OPERATION
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

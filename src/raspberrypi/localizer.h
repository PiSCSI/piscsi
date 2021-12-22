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
#include <map>

using namespace std;

enum LocalizationKey {
	ERROR_AUTHENTICATION
};

class Localizer
{
public:

	Localizer();
	~Localizer() {};

	string Localize(LocalizationKey, const string&);

private:

	void Add(const LocalizationKey, const string&, const string&);
	map<LocalizationKey, map<string, string>> localized_messages;
};

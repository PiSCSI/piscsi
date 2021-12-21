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

class Localizer
{
public:

	Localizer();
	~Localizer() {};

	string Localize(const string&, const string&);

private:

	void Add(const string&, const string&, const string&);
	map<string, map<string, string>> localized_messages;
};

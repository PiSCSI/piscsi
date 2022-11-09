//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <string>

using namespace std;

namespace ras_util
{
	static const char COMPONENT_SEPARATOR = ':';

	bool GetAsInt(const string&, int&);
	string ProcessId(const string&, int, int&, int&);
	string Banner(const string&);

	string GetExtensionLowerCase(const string&);

	void FixCpu(int);
}

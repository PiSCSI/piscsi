//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <string>

using namespace std;

namespace piscsi_util
{
	// Separator for compound options like ID:LUN
	static const char COMPONENT_SEPARATOR = ':';

	bool GetAsUnsignedInt(const string&, int&);
	string ProcessId(const string&, int, int&, int&);
	string Banner(const string&);

	string GetExtensionLowerCase(const string&);

	void FixCpu(int);
}

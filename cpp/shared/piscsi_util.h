//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2021-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <string>
#include <sstream>

using namespace std;

namespace piscsi_util
{
	string Join(const auto& collection, const string_view separator = ", ") {
		ostringstream s;

		for (const auto& element : collection) {
			if (s.tellp()) {
				s << separator;
			}

			s << element;
		}

		return s.str();
	}

	// Separator for compound options like ID:LUN
	static const char COMPONENT_SEPARATOR = ':';

	bool GetAsUnsignedInt(const string&, int&);
	string ProcessId(const string&, int, int&, int&);
	string Banner(string_view);

	string GetExtensionLowerCase(string_view);

	void LogErrno(const string&);

	void FixCpu(int);
}

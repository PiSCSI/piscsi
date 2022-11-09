//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
// Helper methods used by rascsi and rasctl
//
//---------------------------------------------------------------------------

#pragma once

#include <string>

using namespace std;

namespace ras_util
{
	bool GetAsInt(const string&, int&);
	string Banner(const string&);

	string GetExtensionLowerCase(const string&);

	void FixCpu(int);
}

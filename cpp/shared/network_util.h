//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <string>
#include <vector>

using namespace std;

namespace network_util
{
	bool IsInterfaceUp(const string&);
	vector<string> GetNetworkInterfaces();
}

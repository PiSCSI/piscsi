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
#include <set>

using namespace std;

namespace network_util
{
	bool IsInterfaceUp(const string&);
	set<string> GetNetworkInterfaces();
}

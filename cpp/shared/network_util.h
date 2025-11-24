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

struct sockaddr_in;

namespace network_util
{
bool IsInterfaceUp(const string&);
set<string, less<>> GetNetworkInterfaces();
bool ResolveHostName(const string&, sockaddr_in *);
}

//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
// Shared code for SCSI command implementations
//
//---------------------------------------------------------------------------

#pragma once

#include "os.h"
#include <vector>
#include <map>

using namespace std;

namespace scsi_command_util
{
	void ModeSelect(const vector<int>&, const BYTE *, int, int);
	void EnrichFormatPage(map<int, vector<byte>>&, bool, int);
	void AddAppleVendorModePage(map<int, vector<byte>>&, bool);
}

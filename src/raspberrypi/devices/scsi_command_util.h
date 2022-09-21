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

#include <vector>
#include <map>

using namespace std;

namespace scsi_command_util
{
	void ModeSelect(const vector<int>&, const BYTE *, int, int);
	void EnrichFormatPage(map<int, vector<byte>>&, bool, int);
	void AddAppleVendorModePage(map<int, vector<byte>>&, bool);

	int GetInt16(const vector<int>&, int);
	uint32_t GetInt32(const vector<int>&, int);
	uint64_t GetInt64(const vector<int>&, int);
	void SetInt16(BYTE *, int);
	void SetInt32(BYTE *, uint32_t);
	void SetInt64(BYTE *, uint64_t);
	void SetInt16(vector<byte>&, int, int);
	void SetInt32(vector<byte>&, int, int);
}

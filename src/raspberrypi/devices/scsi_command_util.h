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

#include "scsi.h"
#include <vector>
#include <map>

using namespace std;

namespace scsi_command_util
{
	void ModeSelect(scsi_defs::scsi_command, const vector<int>&, const vector<BYTE>&, int, int);
	void EnrichFormatPage(map<int, vector<byte>>&, bool, int);
	void AddAppleVendorModePage(map<int, vector<byte>>&, bool);

	int GetInt16(const vector<BYTE>&, int);
	int GetInt16(const vector<int>&, int);
	int GetInt24(const vector<int>&, int);
	uint32_t GetInt32(const vector<int>&, int);
	uint64_t GetInt64(const vector<int>&, int);
	void SetInt16(vector<byte>&, int, int);
	void SetInt32(vector<byte>&, int, uint32_t);
	void SetInt16(vector<BYTE>&, int, int);
	void SetInt32(vector<BYTE>&, int, uint32_t);
	void SetInt64(vector<BYTE>&, int, uint64_t);
}

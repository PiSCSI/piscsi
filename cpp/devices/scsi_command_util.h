//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
// Shared code for SCSI command implementations
//
//---------------------------------------------------------------------------

#pragma once

#include "shared/scsi.h"
#include <cstdint>
#include <cassert>
#include <span>
#include <vector>
#include <map>

using namespace std;

namespace scsi_command_util
{
	string ModeSelect(scsi_defs::scsi_command, cdb_t, span<const uint8_t>, int, int);
	void EnrichFormatPage(map<int, vector<byte>>&, bool, int);
	void AddAppleVendorModePage(map<int, vector<byte>>&, bool);

	int GetInt16(const auto buf, int offset)
	{
		assert(buf.size() > static_cast<size_t>(offset) + 1);

		return (static_cast<int>(buf[offset]) << 8) | buf[offset + 1];
	}

	template<typename T>
	void SetInt16(vector<T>& buf, int offset, int value)
	{
		assert(buf.size() > static_cast<size_t>(offset) + 1);

		buf[offset] = static_cast<T>(value >> 8);
		buf[offset + 1] = static_cast<T>(value);
	}

	template<typename T>
	void SetInt32(vector<T>& buf, int offset, uint32_t value)
	{
		assert(buf.size() > static_cast<size_t>(offset) + 3);

		buf[offset] = static_cast<T>(value >> 24);
		buf[offset + 1] = static_cast<T>(value >> 16);
		buf[offset + 2] = static_cast<T>(value >> 8);
		buf[offset + 3] = static_cast<T>(value);
	}
    template<typename T>
    void SetInt24(vector<T>& buf, int offset, uint32_t value)
    {
        assert(buf.size() > static_cast<size_t>(offset) + 3);

        buf[offset + 0] = static_cast<T>(value >> 16);
        buf[offset + 1] = static_cast<T>(value >> 8);
        buf[offset + 2] = static_cast<T>(value);
    }

	inline int GetInt24(const auto buf, int offset)
	{
		assert(buf.size() > static_cast<size_t>(offset) + 2);

		return (int(buf[offset]) << 16) | (int(buf[offset + 1]) << 8) | buf[offset + 2];
	}
	uint32_t GetInt32(span <const int>, int);
	uint64_t GetInt64(span<const int>, int);
	void SetInt64(vector<uint8_t>&, int, uint64_t);
}

//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI
//	for Raspberry Pi
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//	Copyright (C) 2022-2023 Uwe Seimet
//	Copyright (C) akuker
//
//  Licensed under the BSD 3-Clause License.
//  See LICENSE file in the project root folder.
//
//---------------------------------------------------------------------------

#pragma once

#include "disk.h"
#include <span>
#include <vector>
#include <map>

using Geometry = pair<uint32_t, uint32_t>;

class SCSIMO : public Disk
{
public:

	explicit SCSIMO(int);
	~SCSIMO() override = default;

	void Open() override;

	vector<uint8_t> InquiryInternal() const override;
	void ModeSelect(scsi_defs::scsi_command, cdb_t, span<const uint8_t>, int) const override;

protected:

	void SetUpModePages(map<int, vector<byte>>&, int, bool) const override;
	void AddFormatPage(map<int, vector<byte>>&, bool) const override;
	void AddVendorPage(map<int, vector<byte>>&, int, bool) const override;

private:

	void AddOptionPage(map<int, vector<byte>>&, bool) const;

	bool SetGeometryForCapacity(uint64_t);

	// The mapping of supported capacities to block sizes and block counts, empty if there is no capacity restriction
	unordered_map<uint64_t, Geometry> geometries;
};

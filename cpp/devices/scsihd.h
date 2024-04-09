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
//	Licensed under the BSD 3-Clause License.
//	See LICENSE file in the project root folder.
//
//---------------------------------------------------------------------------

#pragma once

#include "shared/scsi.h"
#include "disk.h"
#include <string>
#include <span>
#include <vector>
#include <map>

class SCSIHD : public Disk
{
	const string DEFAULT_PRODUCT = "SCSI HD";

public:

	SCSIHD(int, bool, scsi_defs::scsi_level, const unordered_set<uint32_t>& = { 512, 1024, 2048, 4096 });
	~SCSIHD() override = default;

	void FinalizeSetup(off_t);

	void Open() override;

	// Commands
	vector<uint8_t> InquiryInternal() const override;
	void ModeSelect(scsi_defs::scsi_command, cdb_t, span<const uint8_t>, int) const override;

	void AddFormatPage(map<int, vector<byte>>&, bool) const override;
	void AddVendorPage(map<int, vector<byte>>&, int, bool) const override;

private:

	string GetProductData() const;

	scsi_defs::scsi_level scsi_level;
};

//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//	Copyright (C) akuker
//
//	Licensed under the BSD 3-Clause License.
//	See LICENSE file in the project root folder.
//
//  [ SCSI hard disk ]
//
//---------------------------------------------------------------------------

#pragma once

#include "disk.h"
#include <string>

class SCSIHD : public Disk
{
	const string DEFAULT_PRODUCT = "SCSI HD";

public:

	SCSIHD(int, const unordered_set<uint32_t>&, bool, scsi_defs::scsi_level = scsi_level::SCSI_2);
	~SCSIHD() override = default;

	void FinalizeSetup(off_t, off_t = 0);

	void Open() override;

	// Commands
	vector<byte> InquiryInternal() const override;
	void ModeSelect(scsi_defs::scsi_command, const vector<int>&, const vector<BYTE>&, int) const override;

	void AddFormatPage(map<int, vector<byte>>&, bool) const override;
	void AddVendorPage(map<int, vector<byte>>&, int, bool) const override;

protected:

	bool SupportsSaveParameters() const override { return true; }

private:

	string GetProductData() const;

	using super = Disk;

	scsi_defs::scsi_level scsi_level;
};

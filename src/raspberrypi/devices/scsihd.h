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
#include "file_support.h"
#include "filepath.h"

class SCSIHD : public Disk, public FileSupport
{
	static constexpr const char *DEFAULT_PRODUCT = "SCSI HD";

public:

	SCSIHD(const unordered_set<uint32_t>&, bool, scsi_defs::scsi_level = scsi_level::SCSI_2);
	~SCSIHD() override = default;
	SCSIHD(SCSIHD&) = delete;
	SCSIHD& operator=(const SCSIHD&) = delete;

	void FinalizeSetup(const Filepath&, off_t);

	void Open(const Filepath&) override;

	// Commands
	vector<byte> InquiryInternal() const override;
	void ModeSelect(const vector<int>&, const BYTE *, int) const override;

	void AddFormatPage(map<int, vector<byte>>&, bool) const override;
	void AddVendorPage(map<int, vector<byte>>&, int, bool) const override;

private:

	scsi_defs::scsi_level scsi_level;
};

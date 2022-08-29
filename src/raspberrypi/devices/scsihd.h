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

#include "os.h"
#include "disk.h"
#include "filepath.h"

class SCSIHD : public Disk, public FileSupport
{
public:
	SCSIHD(const unordered_set<uint32_t>&, bool);
	virtual ~SCSIHD() {}

	void FinalizeSetup(const Filepath&, off_t);

	void Reset();
	virtual void Open(const Filepath&) override;

	// Commands
	virtual vector<BYTE> InquiryInternal() const override;
	void ModeSelect(const DWORD *cdb, const BYTE *buf, int length) override;

	void AddFormatPage(map<int, vector<BYTE>>&, bool) const override;
	void AddVendorPage(map<int, vector<BYTE>>&, int, bool) const override;
};

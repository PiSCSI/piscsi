//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
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
	SCSIHD(bool);
	virtual ~SCSIHD() {};

	void FinalizeSetup(const Filepath&, off_t);

	void Reset();
	virtual void Open(const Filepath&) override;

	// Commands
	virtual int Inquiry(const DWORD *cdb, BYTE *buf) override;
	bool ModeSelect(const DWORD *cdb, const BYTE *buf, int length) override;

	// Add vendor special page
	int AddVendorPage(int page, bool change, BYTE *buf) override;
};

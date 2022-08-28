//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//  	Copyright (C) akuker
//
//  	Licensed under the BSD 3-Clause License. 
//  	See LICENSE file in the project root folder.
//
//  	[ SCSI NEC "Genuine" Hard Disk]
//
//---------------------------------------------------------------------------
#pragma once

#include "scsihd.h"

//===========================================================================
//
//	SCSI hard disk (PC-9801-55 NEC genuine / Anex86 / T98Next)
//
//===========================================================================
class SCSIHD_NEC : public SCSIHD
{
public:
	SCSIHD_NEC(const unordered_set<uint32_t>&);
	~SCSIHD_NEC() {}

	void Open(const Filepath& path) override;

	// Commands
	vector<BYTE> InquiryInternal() const override;

	void AddErrorPage(map<int, vector<BYTE>>&, bool) const override;
	void AddFormatPage(map<int, vector<BYTE>>&, bool) const override;
	void AddDrivePage(map<int, vector<BYTE>>&, bool) const override;

private:
	// Geometry data
	int cylinders = 0;
	int heads = 0;
	int sectors = 0;
};

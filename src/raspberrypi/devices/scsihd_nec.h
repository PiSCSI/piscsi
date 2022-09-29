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
#include <unordered_set>
#include <map>

using namespace std; //NOSONAR Not relevant for rascsi

//===========================================================================
//
//	SCSI hard disk (PC-9801-55 NEC genuine / Anex86 / T98Next)
//
//===========================================================================
class SCSIHD_NEC : public SCSIHD
{
public:

	SCSIHD_NEC(int id, int lun) : SCSIHD(id, lun, sector_sizes, false) {}
	~SCSIHD_NEC() override = default;

	void Open(const Filepath&) override;

	vector<byte> InquiryInternal() const override;

	void AddErrorPage(map<int, vector<byte>>&, bool) const override;
	void AddFormatPage(map<int, vector<byte>>&, bool) const override;
	void AddDrivePage(map<int, vector<byte>>&, bool) const override;

private:

	static const unordered_set<uint32_t> sector_sizes;

	// Image file offset (NEC only)
	off_t image_offset = 0;

	// Geometry data
	int cylinders = 0;
	int heads = 0;
	int sectors = 0;
};

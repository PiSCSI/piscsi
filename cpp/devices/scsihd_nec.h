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
//  	[ SCSI NEC Compatible Hard Disk]
//
//---------------------------------------------------------------------------

#pragma once

#include "scsihd.h"
#include <unordered_set>
#include <map>
#include <array>
#include <vector>

using namespace std;

//===========================================================================
//
//	SCSI hard disk (PC-9801-55 NEC compatible / Anex86 / T98Next)
//
//===========================================================================
class SCSIHD_NEC : public SCSIHD //NOSONAR The inheritance hierarchy depth is acceptable in this case
{
public:

	explicit SCSIHD_NEC(int lun) : SCSIHD(lun, sector_sizes, false) {}
	~SCSIHD_NEC() override = default;

	void Open() override;

protected:

	vector<byte> InquiryInternal() const override;

	void AddFormatPage(map<int, vector<byte>>&, bool) const override;
	void AddDrivePage(map<int, vector<byte>>&, bool) const override;

private:

	pair<int, int> SetParameters(const array<char, 512>&, int);

	static int GetInt16LittleEndian(const uint8_t *);
	static int GetInt32LittleEndian(const uint8_t *);

	static inline const unordered_set<uint32_t> sector_sizes = { 512 };

	// Image file offset
	off_t image_offset = 0;

	// Geometry data
	int cylinders = 0;
	int heads = 0;
	int sectors = 0;
};

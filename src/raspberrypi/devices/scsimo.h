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
//  	[ SCSI Magneto-Optical Disk]
//
//---------------------------------------------------------------------------

#pragma once

#include "disk.h"
#include "file_support.h"
#include "filepath.h"

class SCSIMO : public Disk, public FileSupport
{
public:

	SCSIMO(const unordered_set<uint32_t>&, const unordered_map<uint64_t, Geometry>&);
	~SCSIMO() override = default;
	SCSIMO(SCSIMO&) = delete;
	SCSIMO& operator=(const SCSIMO&) = delete;

	void Open(const Filepath&) override;

	vector<byte> InquiryInternal() const override;
	void ModeSelect(const vector<int>&, const BYTE *, int) const override;

protected:

	void SetUpModePages(map<int, vector<byte>>&, int, bool) const override;
	void AddFormatPage(map<int, vector<byte>>&, bool) const override;
	void AddVendorPage(map<int, vector<byte>>&, int, bool) const override;

private:

	void AddOptionPage(map<int, vector<byte>>&, bool) const;

	void SetGeometries(const unordered_map<uint64_t, Geometry>& g) { geometries = g; }
	bool SetGeometryForCapacity(uint64_t);

	// The mapping of supported capacities to block sizes and block counts, empty if there is no capacity restriction
	unordered_map<uint64_t, Geometry> geometries;
};

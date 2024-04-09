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

#include "shared/piscsi_exceptions.h"
#include "scsi_command_util.h"
#include "scsihd.h"

using namespace scsi_command_util;

SCSIHD::SCSIHD(int lun, bool removable, scsi_defs::scsi_level level, const unordered_set<uint32_t>& sector_sizes)
	: Disk(removable ? SCRM : SCHD, lun, sector_sizes), scsi_level(level)
{
	SetProtectable(true);
	SetRemovable(removable);
	SetLockable(removable);

	SupportsSaveParameters(true);
}

string SCSIHD::GetProductData() const
{
	uint64_t capacity = GetBlockCount() * GetSectorSizeInBytes();
	string unit;

	// 10,000 MiB and more
	if (capacity >= 10'485'760'000) {
		capacity /= 1'073'741'824;
		unit = "GiB";
	}
	// 1 MiB and more
	else if (capacity >= 1'048'576) {
		capacity /= 1'048'576;
		unit = "MiB";
	}
	else {
		capacity /= 1024;
		unit = "KiB";
	}

	return DEFAULT_PRODUCT + " " + to_string(capacity) + " " + unit;
}

void SCSIHD::FinalizeSetup(off_t image_offset)
{
	Disk::ValidateFile();

	// For non-removable media drives set the default product name based on the drive capacity
	if (!IsRemovable()) {
		SetProduct(GetProductData(), false);
	}

	SetUpCache(image_offset);
}

void SCSIHD::Open()
{
	assert(!IsReady());

	const off_t size = GetFileSize();

	// Sector size (default 512 bytes) and number of blocks
	SetSectorSizeInBytes(GetConfiguredSectorSize() ? GetConfiguredSectorSize() : 512);
	SetBlockCount(static_cast<uint32_t>(size >> GetSectorSizeShiftCount()));

	FinalizeSetup(0);
}

vector<uint8_t> SCSIHD::InquiryInternal() const
{
	return HandleInquiry(device_type::direct_access, scsi_level, IsRemovable());
}

void SCSIHD::ModeSelect(scsi_command cmd, cdb_t cdb, span<const uint8_t> buf, int length) const
{
	if (const string result = scsi_command_util::ModeSelect(cmd, cdb, buf, length, 1 << GetSectorSizeShiftCount());
		!result.empty()) {
		LogWarn(result);
	}
}

void SCSIHD::AddFormatPage(map<int, vector<byte>>& pages, bool changeable) const
{
	Disk::AddFormatPage(pages, changeable);

	EnrichFormatPage(pages, changeable, 1 << GetSectorSizeShiftCount());
}

void SCSIHD::AddVendorPage(map<int, vector<byte>>& pages, int page, bool changeable) const
{
	// Page code 48
	if (page == 0x30 || page == 0x3f) {
		AddAppleVendorModePage(pages, changeable);
	}
}

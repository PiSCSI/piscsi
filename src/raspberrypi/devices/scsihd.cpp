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
//	[ SCSI hard disk ]
//
//---------------------------------------------------------------------------

#include "scsihd.h"
#include "fileio.h"
#include "rascsi_exceptions.h"
#include "scsi_command_util.h"

using namespace scsi_command_util;

SCSIHD::SCSIHD(int lun, const unordered_set<uint32_t>& sector_sizes, bool removable, scsi_defs::scsi_level level)
	: Disk(removable ? SCRM : SCHD, lun), scsi_level(level)
{
	SetSectorSizes(sector_sizes);
}

string SCSIHD::GetProductData() const
{
	uint64_t capacity = GetBlockCount() * GetSectorSizeInBytes();
	string unit;

	// 10 GiB and more
	if (capacity >= 1'099'511'627'776) {
		capacity /= 1'099'511'627'776;
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

void SCSIHD::FinalizeSetup(off_t size, off_t image_offset)
{
	// Effective size must be a multiple of the sector size
	size = (size / GetSectorSizeInBytes()) * GetSectorSizeInBytes();

	// 2 TiB is the current maximum
	if (size > 2LL * 1024 * 1024 * 1024 * 1024) {
		throw io_exception("Drive capacity cannot exceed 2 TiB");
	}

	// For non-removable media drives set the default product name based on the drive capacity
	if (!IsRemovable()) {
		SetProduct(GetProductData(), false);
	}

	super::ValidateFile(GetFilename());

	SetUpCache(image_offset);
}

void SCSIHD::Open()
{
	assert(!IsReady());

	off_t size = GetFileSize();

	// Sector size (default 512 bytes) and number of blocks
	SetSectorSizeInBytes(GetConfiguredSectorSize() ? GetConfiguredSectorSize() : 512);
	SetBlockCount((uint32_t)(size >> GetSectorSizeShiftCount()));

	FinalizeSetup(size);
}

vector<byte> SCSIHD::InquiryInternal() const
{
	return HandleInquiry(device_type::DIRECT_ACCESS, scsi_level, IsRemovable());
}

void SCSIHD::ModeSelect(scsi_command cmd, const vector<int>& cdb, const vector<BYTE>& buf, int length) const
{
	scsi_command_util::ModeSelect(cmd, cdb, buf, length, 1 << GetSectorSizeShiftCount());
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

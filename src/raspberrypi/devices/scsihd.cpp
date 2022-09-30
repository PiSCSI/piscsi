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
#include <sstream>

using namespace scsi_command_util;

SCSIHD::SCSIHD(int lun, const unordered_set<uint32_t>& sector_sizes, bool removable, scsi_defs::scsi_level level)
	: Disk(removable ? "SCRM" : "SCHD", lun)
{
	scsi_level = level;

	SetSectorSizes(sector_sizes);
}

void SCSIHD::FinalizeSetup(const Filepath &path, off_t size, off_t image_offset)
{
    // 2TB is the current maximum
	if (size > 2LL * 1024 * 1024 * 1024 * 1024) {
		throw io_exception("File size must not exceed 2 TiB");
	}

	// For non-removable media drives set the default product name based on the drive capacity
	if (!IsRemovable()) {
		uint64_t capacity = GetBlockCount() * GetSectorSizeInBytes();
		string unit;
		// 10 GiB and more
		if (capacity >= 1099511627776) {
			capacity /= 1099511627776;
			unit = "GiB";
		}
		// 1 MiB and more
		else if (capacity >= 1048576) {
			capacity /= 1048576;
			unit = "MiB";
		}
		else {
			capacity /= 1024;
			unit = "KiB";
		}
		stringstream product;
		product << DEFAULT_PRODUCT << " " << capacity << " " << unit;
		SetProduct(product.str(), false);
	}

	SetReadOnly(false);
	SetProtectable(true);
	SetProtected(false);

	Disk::Open(path);
	FileSupport::SetPath(path);

	SetUpCache(path, image_offset);
}

void SCSIHD::Open(const Filepath& path)
{
	assert(!IsReady());

	// Open as read-only
	Fileio fio;
	if (!fio.Open(path, Fileio::OpenMode::ReadOnly)) {
		throw file_not_found_exception("Can't open SCSI hard disk file");
	}

	// Get file size
	off_t size = fio.GetFileSize();
	fio.Close();

	// Sector size (default 512 bytes) and number of blocks
	SetSectorSizeInBytes(GetConfiguredSectorSize() ? GetConfiguredSectorSize() : 512);
	SetBlockCount((DWORD)(size >> GetSectorSizeShiftCount()));

	// Effective size must be a multiple of the sector size
	size = (size / GetSectorSizeInBytes()) * GetSectorSizeInBytes();

	FinalizeSetup(path, size);
}

vector<byte> SCSIHD::InquiryInternal() const
{
	return HandleInquiry(device_type::DIRECT_ACCESS, scsi_level, IsRemovable());
}

void SCSIHD::ModeSelect(const vector<int>& cdb, const vector<BYTE>& buf, int length) const
{
	scsi_command_util::ModeSelect(cdb, buf, length, 1 << GetSectorSizeShiftCount());
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

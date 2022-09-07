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

static const char *DEFAULT_PRODUCT = "SCSI HD";

SCSIHD::SCSIHD(const unordered_set<uint32_t>& sector_sizes, bool removable, scsi_defs::scsi_level level)
	: Disk(removable ? "SCRM" : "SCHD")
{
	this->scsi_level = level;

	SetSectorSizes(sector_sizes);
}

void SCSIHD::FinalizeSetup(const Filepath &path, off_t size)
{
    // 2TB is the current maximum
	if (size > 2LL * 1024 * 1024 * 1024 * 1024) {
		throw io_exception("File size must not exceed 2 TiB");
	}

	// For non-removable media drives set the default product name based on the drive capacity
	if (!IsRemovable()) {
		uint64_t capacity = GetBlockCount() * GetSectorSizeInBytes();
		string unit;
		if (capacity >= 1000000) {
			capacity /= 1000000;
			unit = "MB";
		}
		else {
			capacity /= 1000;
			unit = "KB";
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
}

void SCSIHD::Reset()
{
	// Unlock and release attention
	SetLocked(false);
	SetAttn(false);

	// No reset, clear code
	SetReset(false);
	SetStatusCode(0);
}

void SCSIHD::Open(const Filepath& path)
{
	assert(!IsReady());

	// Open as read-only
	Fileio fio;
	if (!fio.Open(path, Fileio::ReadOnly)) {
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

vector<BYTE> SCSIHD::InquiryInternal() const
{
	return HandleInquiry(device_type::DIRECT_ACCESS, scsi_level, IsRemovable());
}

void SCSIHD::ModeSelect(const DWORD *cdb, const BYTE *buf, int length)
{
	scsi_command_util::ModeSelect(cdb, buf, length, 1 << GetSectorSizeShiftCount());
}

void SCSIHD::AddFormatPage(map<int, vector<BYTE>>& pages, bool changeable) const
{
	Disk::AddFormatPage(pages, changeable);

	scsi_command_util::EnrichFormatPage(pages, changeable, 1 << GetSectorSizeShiftCount());
}

//---------------------------------------------------------------------------
//
//	Add Vendor special page to make drive Apple compatible
//
//---------------------------------------------------------------------------
void SCSIHD::AddVendorPage(map<int, vector<BYTE>>& pages, int page, bool changeable) const
{
	// Page code 48
	if (page != 0x30 && page != 0x3f) {
		return;
	}

	vector<BYTE> buf(30);

	// No changeable area
	if (!changeable) {
		memcpy(&buf[0xa], "APPLE COMPUTER, INC.", 20);
	}

	pages[48] = buf;
}

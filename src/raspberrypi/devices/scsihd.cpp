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
//	[ SCSI hard disk ]
//
//---------------------------------------------------------------------------
#include "scsihd.h"
#include "fileio.h"
#include "exceptions.h"
#include <sstream>

#define DEFAULT_PRODUCT "SCSI HD"

//===========================================================================
//
//	SCSI Hard Disk
//
//===========================================================================

SCSIHD::SCSIHD(const unordered_set<uint32_t>& sector_sizes, bool removable) : Disk(removable ? "SCRM" : "SCHD")
{
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
	return HandleInquiry(device_type::DIRECT_ACCESS, scsi_level::SCSI_2, IsRemovable());
}


// TODO This is a duplicate of code in scsimo.cpp
void SCSIHD::ModeSelect(const DWORD *cdb, const BYTE *buf, int length)
{
	assert(length >= 0);

	// PF
	if (cdb[1] & 0x10) {
		// Mode Parameter header
		if (length >= 12) {
			// Check the block length bytes
			int size = 1 << GetSectorSizeShiftCount();
			if (buf[9] != (BYTE)(size >> 16) || buf[10] != (BYTE)(size >> 8) || buf[11] != (BYTE)size) {
				// currently does not allow changing sector length
				throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_PARAMETER_LIST);
			}
			buf += 12;
			length -= 12;
		}

		// Parsing the page
		while (length > 0) {
			int page = buf[0];

			switch (page) {
				// Format device page
				case 0x03: {
					// With this page the sector size for a subsequent FORMAT can be selected, but only very few
					// drives support this, e.g FUJITSU M2624S
					// We are fine as long as the current sector size remains unchanged
					int current_size = GetSectorSizeInBytes();
					if (buf[0xc] != (BYTE)(current_size >> 8) || buf[0xd] != (BYTE)current_size) {
						// With rascsi it is not possible to permanently (by formatting) change the sector size,
						// because the size is an externally configurable setting only
						throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_PARAMETER_LIST);
					}
					}
					break;

				// vendor unique format
				case 0x20:
					// TODO Is it correct to ignore this? The initiator would rely on something to actually have been changed.
					// just ignore, for now
					break;

				default:
                    LOGTRACE("Unknown MODE SELECT page code: $%02X", page);
					break;
			}

			// Advance to the next page
			int size = buf[1] + 2;
			length -= size;
			buf += size;
		}
	}
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

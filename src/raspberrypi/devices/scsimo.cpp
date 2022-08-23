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
//	[ SCSI Magneto-Optical Disk]
//
//---------------------------------------------------------------------------

#include "fileio.h"
#include "exceptions.h"
#include "scsimo.h"

SCSIMO::SCSIMO(const unordered_set<uint32_t>& sector_sizes, const unordered_map<uint64_t, Geometry>& geometries) : Disk("SCMO")
{
	SetSectorSizes(sector_sizes);
	SetGeometries(geometries);
}

void SCSIMO::Open(const Filepath& path)
{
	assert(!IsReady());

	// Open as read-only
	Fileio fio;

	if (!fio.Open(path, Fileio::ReadOnly)) {
		throw file_not_found_exception("Can't open MO file");
	}

	// Get file size
	off_t size = fio.GetFileSize();
	fio.Close();

	// For some priorities there are hard-coded, well-defined sector sizes and block counts
	// TODO Find a more flexible solution
	if (!SetGeometryForCapacity(size)) {
		// Sector size (default 512 bytes) and number of blocks
		SetSectorSizeInBytes(GetConfiguredSectorSize() ? GetConfiguredSectorSize() : 512);
		SetBlockCount(size >> GetSectorSizeShiftCount());
	}

	// Effective size must be a multiple of the sector size
	size = (size / GetSectorSizeInBytes()) * GetSectorSizeInBytes();

	SetReadOnly(false);
	SetProtectable(true);
	SetProtected(false);

	Disk::Open(path);
	FileSupport::SetPath(path);

	// Attention if ready
	if (IsReady()) {
		SetAttn(true);
	}
}

vector<BYTE> SCSIMO::InquiryInternal() const
{
	return HandleInquiry(device_type::OPTICAL_MEMORY, scsi_level::SCSI_2, true);
}

void SCSIMO::SetDeviceParameters(BYTE *buf)
{
	Disk::SetDeviceParameters(buf);

	// MEDIUM TYPE: Optical reversible or erasable
	buf[2] = 0x03;
}

void SCSIMO::AddModePages(map<int, vector<BYTE>>& pages, int page, bool changeable) const
{
	Disk::AddModePages(pages, page, changeable);

	// Page code 6
	if (page == 0x06 || page == 0x3f) {
		AddOptionPage(pages, changeable);
	}
}

void SCSIMO::AddOptionPage(map<int, vector<BYTE>>& pages, bool) const
{
	vector<BYTE> buf(4);
	pages[6] = buf;

	// Do not report update blocks
}

void SCSIMO::ModeSelect(const DWORD *cdb, const BYTE *buf, int length)
{
	int size;

	ASSERT(length >= 0);

	// PF
	if (cdb[1] & 0x10) {
		// Mode Parameter header
		if (length >= 12) {
			// Check the block length (in bytes)
			size = 1 << GetSectorSizeShiftCount();
			if (buf[9] != (BYTE)(size >> 16) ||
				buf[10] != (BYTE)(size >> 8) || buf[11] != (BYTE)size) {
				// Currently does not allow changing sector length
				throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_PARAMETER_LIST);
			}
			buf += 12;
			length -= 12;
		}

		// Parsing the page
		while (length > 0) {
			// Get the page
			int page = buf[0];

			switch (page) {
				// format device
				case 0x03:
					// Check the number of bytes in the physical sector
					size = 1 << GetSectorSizeShiftCount();
					if (buf[0xc] != (BYTE)(size >> 8) ||
						buf[0xd] != (BYTE)size) {
						// Currently does not allow changing sector length
						throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_PARAMETER_LIST);
					}
					break;
				// vendor unique format
				case 0x20:
					// just ignore, for now
					break;

				// Other page
				default:
					break;
			}

			// Advance to the next page
			size = buf[1] + 2;
			length -= size;
			buf += size;
		}
	}
}

//---------------------------------------------------------------------------
//
//	Vendor Unique Format Page 20h (MO)
//
//---------------------------------------------------------------------------
void SCSIMO::AddVendorPage(map<int, vector<BYTE>>& pages, int page, bool changeable) const
{
	// Page code 20h
	if (page != 0x20 && page != 0x3f) {
		return;
	}

	vector<BYTE> buf(12);

	// No changeable area
	if (changeable) {
		pages[32] = buf;

		return;
	}

	/*
		mode page code 20h - Vendor Unique Format Page
		format mode XXh type 0
		information: http://h20628.www2.hp.com/km-ext/kmcsdirect/emr_na-lpg28560-1.pdf

		offset  description
		  02h   format mode
		  03h   type of format (0)
		04~07h  size of user band (total sectors?)
		08~09h  size of spare band (spare sectors?)
		0A~0Bh  number of bands

		actual value of each 3.5inches optical medium (grabbed by Fujitsu M2513EL)

		                     128M     230M    540M    640M
		---------------------------------------------------
		size of user band   3CBFAh   6CF75h  FE45Ch  4BC50h
		size of spare band   0400h    0401h   08CAh   08C4h
		number of bands      0001h    000Ah   0012h   000Bh

		further information: http://r2089.blog36.fc2.com/blog-entry-177.html
	*/

	if (IsReady()) {
		unsigned spare = 0;
		unsigned bands = 0;
		uint64_t blocks = GetBlockCount();

		if (GetSectorSizeInBytes() == 512) {
			switch (blocks) {
				// 128MB
				case 248826:
					spare = 1024;
					bands = 1;
					break;

				// 230MB
				case 446325:
					spare = 1025;
					bands = 10;
					break;

				// 540MB
				case 1041500:
					spare = 2250;
					bands = 18;
					break;
			}
		}

		if (GetSectorSizeInBytes() == 2048) {
			switch (blocks) {
				// 640MB
				case 310352:
					spare = 2244;
					bands = 11;
					break;

					// 1.3GB (lpproj: not tested with real device)
				case 605846:
					spare = 4437;
					bands = 18;
					break;
			}
		}

		buf[2] = 0; // format mode
		buf[3] = 0; // type of format
		buf[4] = (BYTE)(blocks >> 24);
		buf[5] = (BYTE)(blocks >> 16);
		buf[6] = (BYTE)(blocks >> 8);
		buf[7] = (BYTE)blocks;
		buf[8] = (BYTE)(spare >> 8);
		buf[9] = (BYTE)spare;
		buf[10] = (BYTE)(bands >> 8);
		buf[11] = (BYTE)bands;
	}

	pages[32] = buf;

	return;
}

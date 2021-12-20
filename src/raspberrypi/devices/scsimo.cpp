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

#include "scsimo.h"

#include "../rascsi.h"
#include "fileio.h"
#include "exceptions.h"
#include <sstream>

//===========================================================================
//
//	SCSI magneto-optical disk
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
SCSIMO::SCSIMO() : Disk("SCMO")
{
}

//---------------------------------------------------------------------------
//
//	Open
//
//---------------------------------------------------------------------------
void SCSIMO::Open(const Filepath& path)
{
	ASSERT(!IsReady());

	// Open as read-only
	Fileio fio;

	if (!fio.Open(path, Fileio::ReadOnly)) {
		throw file_not_found_exception("Can't open MO file");
	}

	// Get file size
	off_t size = fio.GetFileSize();
	fio.Close();

	// For some priorities there are hard-coded, well-defined sector sizes and block counts
	if (!SetGeometryForCapacity(size)) {
		// Sector size (default 512 bytes) and number of blocks
		SetSectorSizeInBytes(GetConfiguredSectorSize() ? GetConfiguredSectorSize() : 512, true);
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

//---------------------------------------------------------------------------
//
//	INQUIRY
//
//---------------------------------------------------------------------------
int SCSIMO::Inquiry(const DWORD *cdb, BYTE *buf)
{
	ASSERT(cdb);
	ASSERT(buf);

	// EVPD check
	if (cdb[1] & 0x01) {
		SetStatusCode(STATUS_INVALIDCDB);
		return FALSE;
	}

	// Basic data
	// buf[0] ... Optical Memory Device
	// buf[1] ... Removable
	// buf[2] ... SCSI-2 compliant command system
	// buf[3] ... SCSI-2 compliant Inquiry response
	// buf[4] ... Inquiry additional data
	memset(buf, 0, 8);
	buf[0] = 0x07;
	buf[1] = 0x80;
	buf[2] = 0x02;
	buf[3] = 0x02;
	buf[4] = 36 - 5;	// required

	// Padded vendor, product, revision
	memcpy(&buf[8], GetPaddedName().c_str(), 28);

	// Size return data
	int size = (buf[4] + 5);

	// Limit the size if the buffer is too small
	if (size > (int)cdb[4]) {
		size = (int)cdb[4];
	}

	return size;
}

//---------------------------------------------------------------------------
//
//	MODE SELECT
//
//---------------------------------------------------------------------------
bool SCSIMO::ModeSelect(const DWORD *cdb, const BYTE *buf, int length)
{
	int size;

	ASSERT(buf);
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
				SetStatusCode(STATUS_INVALIDPRM);
				return false;
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
						SetStatusCode(STATUS_INVALIDPRM);
						return false;
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

	// Do not generate an error for the time being (MINIX)
	return true;
}

//---------------------------------------------------------------------------
//
//	Vendor Unique Format Page 20h (MO)
//
//---------------------------------------------------------------------------
int SCSIMO::AddVendor(int page, BOOL change, BYTE *buf)
{
	ASSERT(buf);

	// Page code 20h
	if ((page != 0x20) && (page != 0x3f)) {
		return 0;
	}

	// Set the message length
	buf[0] = 0x20;
	buf[1] = 0x0a;

	// No changeable area
	if (change) {
		return 12;
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

	return 12;
}

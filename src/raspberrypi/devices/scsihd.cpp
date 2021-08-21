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
#include "xm6.h"
#include "fileio.h"
#include "exceptions.h"
#include <sstream>

//===========================================================================
//
//	SCSI Hard Disk
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
SCSIHD::SCSIHD(bool removable) : Disk(removable ? "SCRM" : "SCHD")
{
	SetRemovable(removable);
	SetProtectable(true);
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void SCSIHD::Reset()
{
	// Unlock and release attention
	SetLocked(false);
	SetAttn(false);

	// No reset, clear code
	SetReset(false);
	SetStatusCode(STATUS_NOERROR);
}

//---------------------------------------------------------------------------
//
//	Open
//
//---------------------------------------------------------------------------
void SCSIHD::Open(const Filepath& path)
{
	ASSERT(!IsReady());

	// Open as read-only
	Fileio fio;
	if (!fio.Open(path, Fileio::ReadOnly)) {
		throw io_exception("Can't open hard disk file read-only");
	}

	// Get file size
	off_t size = fio.GetFileSize();
	fio.Close();

	// Must be a multiple of 512 bytes
	if (size & 0x1ff) {
		throw io_exception("File size must be a multiple of 512 bytes");
	}

    // 2TB is the current maximum
	if (size > 2LL * 1024 * 1024 * 1024 * 1024) {
		throw io_exception("File size must not exceed 2 TB");
	}

	// sector size 512 bytes and number of blocks
	SetSectorSize(9);
	SetBlockCount((DWORD)(size >> 9));

	LOGINFO("Media capacity for image file '%s': %d blocks", path.GetPath(),GetBlockCount());

	// Set the default product name based on the drive capacity
	stringstream product;
	product << DEFAULT_PRODUCT << " " << (GetBlockCount() >> 11) << " MB";
	SetProduct(product.str(), false);

	Disk::Open(path);
	FileSupport::SetPath(path);
}

//---------------------------------------------------------------------------
//
//	INQUIRY
//
//---------------------------------------------------------------------------
int SCSIHD::Inquiry(const DWORD *cdb, BYTE *buf)
{
	ASSERT(cdb);
	ASSERT(buf);

	// EVPD check
	if (cdb[1] & 0x01) {
		SetStatusCode(STATUS_INVALIDCDB);
		return 0;
	}

	// Ready check (Error if no image file)
	if (!IsReady()) {
		SetStatusCode(STATUS_NOTREADY);
		return 0;
	}

	// Basic data
	// buf[0] ... Direct Access Device
	// buf[1] ... Bit 7 set means removable
	// buf[2] ... SCSI-2 compliant command system
	// buf[3] ... SCSI-2 compliant Inquiry response
	// buf[4] ... Inquiry additional data
	memset(buf, 0, 8);

	// SCSI-2 p.104 4.4.3 Incorrect logical unit handling
	if (((cdb[1] >> 5) & 0x07) != GetLun()) {
		buf[0] = 0x7f;
	}

	buf[1] = IsRemovable() ? 0x80 : 0x00;
	buf[2] = 0x02;
	buf[3] = 0x02;
	buf[4] = 122 + 3;	// Value close to real HDD

	// Padded vendor, product, revision
	memcpy(&buf[8], GetPaddedName().c_str(), 28);

	// Size of data that can be returned
	int size = (buf[4] + 5);

	// Limit if the other buffer is small
	if (size > (int)cdb[4]) {
		size = (int)cdb[4];
	}

	return size;
}

//---------------------------------------------------------------------------
//
//	MODE SELECT
//	*Not affected by disk.code
//
//---------------------------------------------------------------------------
bool SCSIHD::ModeSelect(const DWORD *cdb, const BYTE *buf, int length)
{
	BYTE page;
	int size;

	ASSERT(buf);
	ASSERT(length >= 0);

	// PF
	if (cdb[1] & 0x10) {
		// Mode Parameter header
		if (length >= 12) {
			// Check the block length bytes
			size = 1 << GetSectorSize();
			if (buf[9] != (BYTE)(size >> 16) ||
				buf[10] != (BYTE)(size >> 8) ||
				buf[11] != (BYTE)size) {
				// currently does not allow changing sector length
				SetStatusCode(STATUS_INVALIDPRM);
				return false;
			}
			buf += 12;
			length -= 12;
		}

		// Parsing the page
		while (length > 0) {
			// Get page
			page = buf[0];

			switch (page) {
				// format device
				case 0x03:
					// check the number of bytes in the physical sector
					size = 1 << GetSectorSize();
					if (buf[0xc] != (BYTE)(size >> 8) ||
						buf[0xd] != (BYTE)size) {
						// currently does not allow changing sector length
						SetStatusCode(STATUS_INVALIDPRM);
						return false;
					}
					break;

                // CD-ROM Parameters
                // According to the SONY CDU-541 manual, Page code 8 is supposed
                // to set the Logical Block Adress Format, as well as the
                // inactivity timer multiplier
                case 0x08:
                    // Debug code for Issue #2:
                    //     https://github.com/akuker/RASCSI/issues/2
                    LOGWARN("[Unhandled page code] Received mode page code 8 with total length %d\n     ", length);
                    for (int i = 0; i<length; i++)
                    {
                        printf("%02X ", buf[i]);
                    }
                    printf("\n");
                    break;
				// Other page
				default:
                    printf("Unknown Mode Select page code received: %02X\n",page);
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
//	Add Vendor special page to make drive Apple compatible
//
//---------------------------------------------------------------------------
int SCSIHD::AddVendor(int page, bool change, BYTE *buf)
{
	ASSERT(buf);

	// Page code 48 or 63
	if (page != 0x30 && page != 0x3f) {
		return 0;
	}

	// Set the message length
	buf[0] = 0x30;
	buf[1] = 0x1c;

	// No changeable area
	if (!change) {
		memcpy(&buf[0xa], "APPLE COMPUTER, INC.", 20);
	}

	return 30;
}

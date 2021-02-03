//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//  Copyright (C) akuker
//
//  Licensed under the BSD 3-Clause License. 
//  See LICENSE file in the project root folder.
//
//  [ SCSI hard disk ]
//
//---------------------------------------------------------------------------
#include "scsihd.h"
#include "xm6.h"
#include "fileio.h"

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
SCSIHD::SCSIHD() : Disk()
{
	// SCSI Hard Disk
	disk.id = MAKEID('S', 'C', 'H', 'D');
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void FASTCALL SCSIHD::Reset()
{
	// Unlock and release attention
	disk.lock = FALSE;
	disk.attn = FALSE;

	// No reset, clear code
	disk.reset = FALSE;
	disk.code = 0x00;
}

//---------------------------------------------------------------------------
//
//	Open
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSIHD::Open(const Filepath& path, BOOL /*attn*/)
{
	Fileio fio;
	off64_t size;

	ASSERT(this);
	ASSERT(!disk.ready);

	// read open required
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}

	// Get file size
	size = fio.GetFileSize();
	fio.Close();

	// Must be 512 bytes
	if (size & 0x1ff) {
		return FALSE;
	}

	// 10MB or more
	if (size < 0x9f5400) {
		return FALSE;
	}
    // 2TB according to xm6i
    // There is a similar one in wxw/wxw_cfg.cpp
	if (size > 2LL * 1024 * 1024 * 1024 * 1024) {
		return FALSE;
	}

	// sector size and number of blocks
	disk.size = 9;
	disk.blocks = (DWORD)(size >> 9);

	// Call base class
	return Disk::Open(path);
}

//---------------------------------------------------------------------------
//
//	INQUIRY
//
//---------------------------------------------------------------------------
int FASTCALL SCSIHD::Inquiry(
	const DWORD *cdb, BYTE *buf, DWORD major, DWORD minor)
{
	char vendor[32];
	char product[32];
	char rev[32];
	int size;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(buf);
	ASSERT(cdb[0] == 0x12);

	// EVPD check
	if (cdb[1] & 0x01) {
		disk.code = DISK_INVALIDCDB;
		return 0;
	}

	// Ready check (Error if no image file)
	if (!disk.ready) {
		disk.code = DISK_NOTREADY;
		return 0;
	}

	// Basic data
	// buf[0] ... Direct Access Device
	// buf[2] ... SCSI-2 compliant command system
	// buf[3] ... SCSI-2 compliant Inquiry response
	// buf[4] ... Inquiry additional data
	memset(buf, 0, 8);

	// SCSI-2 p.104 4.4.3 Incorrect logical unit handling
	if (((cdb[1] >> 5) & 0x07) != disk.lun) {
		buf[0] = 0x7f;
	}

	buf[2] = 0x02;
	buf[3] = 0x02;
	buf[4] = 122 + 3;	// Value close to real HDD

	// Fill with blanks
	memset(&buf[8], 0x20, buf[4] - 3);

	// Determine vendor name/product name
	sprintf(vendor, BENDER_SIGNATURE);
	size = disk.blocks >> 11;
	if (size < 300)
		sprintf(product, "PRODRIVE LPS%dS", size);
	else if (size < 600)
		sprintf(product, "MAVERICK%dS", size);
	else if (size < 800)
		sprintf(product, "LIGHTNING%dS", size);
	else if (size < 1000)
		sprintf(product, "TRAILBRAZER%dS", size);
	else if (size < 2000)
		sprintf(product, "FIREBALL%dS", size);
	else
		sprintf(product, "FBSE%d.%dS", size / 1000, (size % 1000) / 100);

	// Vendor name
	memcpy(&buf[8], vendor, strlen(vendor));

	// Product name
	memcpy(&buf[16], product, strlen(product));

	// Revision
	sprintf(rev, "0%01d%01d%01d",
			(int)major, (int)(minor >> 4), (int)(minor & 0x0f));
	memcpy(&buf[32], rev, 4);

	// Size of data that can be returned
	size = (buf[4] + 5);

	// Limit if the other buffer is small
	if (size > (int)cdb[4]) {
		size = (int)cdb[4];
	}

	//  Success
	disk.code = DISK_NOERROR;
	return size;
}

//---------------------------------------------------------------------------
//
//	MODE SELECT
//	*Not affected by disk.code
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSIHD::ModeSelect(const DWORD *cdb, const BYTE *buf, int length)
{
	BYTE page;
	int size;

	ASSERT(this);
	ASSERT(buf);
	ASSERT(length >= 0);

	// PF
	if (cdb[1] & 0x10) {
		// Mode Parameter header
		if (length >= 12) {
			// Check the block length bytes
			size = 1 << disk.size;
			if (buf[9] != (BYTE)(size >> 16) ||
				buf[10] != (BYTE)(size >> 8) ||
				buf[11] != (BYTE)size) {
				// currently does not allow changing sector length
				disk.code = DISK_INVALIDPRM;
				return FALSE;
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
					size = 1 << disk.size;
					if (buf[0xc] != (BYTE)(size >> 8) ||
						buf[0xd] != (BYTE)size) {
						// currently does not allow changing sector length
						disk.code = DISK_INVALIDPRM;
						return FALSE;
					}
					break;

                // CD-ROM Parameters
                // According to the SONY CDU-541 manual, Page code 8 is supposed
                // to set the Logical Block Adress Format, as well as the
                // inactivity timer multiplier
                case 0x08:
                    // Debug code for Issue #2:
                    //     https://github.com/akuker/RASCSI/issues/2
                    printf("[Unhandled page code] Received mode page code 8 with total length %d\n     ", length);
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
	disk.code = DISK_NOERROR;

	return TRUE;
}

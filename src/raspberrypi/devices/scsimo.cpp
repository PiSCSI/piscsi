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
//  [ SCSI Magneto-Optical Disk]
//
//---------------------------------------------------------------------------

#include "scsimo.h"
#include "xm6.h"
#include "fileio.h"

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
SCSIMO::SCSIMO() : Disk()
{
	// SCSI magneto-optical disk
	disk.id = MAKEID('S', 'C', 'M', 'O');

	// Set as removable
	disk.removable = TRUE;
}

//---------------------------------------------------------------------------
//
//	Open
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSIMO::Open(const Filepath& path, BOOL attn)
{
	Fileio fio;
	off64_t size;

	ASSERT(this);
	ASSERT(!disk.ready);

	// Open as read-only
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}

	// Get file size
	size = fio.GetFileSize();
	fio.Close();

	switch (size) {
		// 128MB
		case 0x797f400:
			disk.size = 9;
			disk.blocks = 248826;
			break;

		// 230MB
		case 0xd9eea00:
			disk.size = 9;
			disk.blocks = 446325;
			break;

		// 540MB
		case 0x1fc8b800:
			disk.size = 9;
			disk.blocks = 1041500;
			break;

		// 640MB
		case 0x25e28000:
			disk.size = 11;
			disk.blocks = 310352;
			break;

		// Other (this is an error)
		default:
			return FALSE;
	}

	// Call the base class
	Disk::Open(path);

	// Attention if ready
	if (disk.ready && attn) {
		disk.attn = TRUE;
	}

	return TRUE;
}

#ifndef RASCSI
//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSIMO::Load(Fileio *fio, int ver)
{
	DWORD sz;
	disk_t buf;
	DWORD padding;
	Filepath path;

	ASSERT(this);
	ASSERT(fio);
	ASSERT(ver >= 0x0200);

	// Prior to version 2.03, the disk was not saved
	if (ver <= 0x0202) {
		return TRUE;
	}

	// load size, match
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != 52) {
		return FALSE;
	}

	// load into buffer
	PROP_IMPORT(fio, buf.id);
	PROP_IMPORT(fio, buf.ready);
	PROP_IMPORT(fio, buf.writep);
	PROP_IMPORT(fio, buf.readonly);
	PROP_IMPORT(fio, buf.removable);
	PROP_IMPORT(fio, buf.lock);
	PROP_IMPORT(fio, buf.attn);
	PROP_IMPORT(fio, buf.reset);
	PROP_IMPORT(fio, buf.size);
	PROP_IMPORT(fio, buf.blocks);
	PROP_IMPORT(fio, buf.lun);
	PROP_IMPORT(fio, buf.code);
	PROP_IMPORT(fio, padding);

	// Load path
	if (!path.Load(fio, ver)) {
		return FALSE;
	}

	// Always eject
	Eject(TRUE);

	// Move only if IDs match
	if (disk.id != buf.id) {
		// Not MO at the time of save. Maintain eject status
		return TRUE;
	}

	// Re-try opening
	if (!Open(path, FALSE)) {
		// Cannot reopen. Maintain eject status
		return TRUE;
	}

	// Disk cache is created in Open. Move property only
	if (!disk.readonly) {
		disk.writep = buf.writep;
	}
	disk.lock = buf.lock;
	disk.attn = buf.attn;
	disk.reset = buf.reset;
	disk.lun = buf.lun;
	disk.code = buf.code;

	// loaded successfully
	return TRUE;
}
#endif	// RASCSI

//---------------------------------------------------------------------------
//
//	INQUIRY
//
//---------------------------------------------------------------------------
int FASTCALL SCSIMO::Inquiry(
	const DWORD *cdb, BYTE *buf, DWORD major, DWORD minor)
{
	int size;
	char rev[32];

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(buf);
	ASSERT(cdb[0] == 0x12);

	// EVPD check
	if (cdb[1] & 0x01) {
		disk.code = DISK_INVALIDCDB;
		return FALSE;
	}

	// 基本データ
	// buf[0] ... Optical Memory Device
	// buf[1] ... Removable
	// buf[2] ... SCSI-2 compliant command system
	// buf[3] ... SCSI-2 compliant Inquiry response
	// buf[4] ... Inquiry additional data
	memset(buf, 0, 8);
	buf[0] = 0x07;

	// SCSI-2 p.104 4.4.3 Incorrect logical unit handling
	if (((cdb[1] >> 5) & 0x07) != disk.lun) {
		buf[0] = 0x7f;
	}

	buf[1] = 0x80;
	buf[2] = 0x02;
	buf[3] = 0x02;
	buf[4] = 36 - 5;	// required

	// Fill with blanks
	memset(&buf[8], 0x20, buf[4] - 3);

	// Vendor name
	memcpy(&buf[8], BENDER_SIGNATURE, strlen(BENDER_SIGNATURE));

	// Product name
	memcpy(&buf[16], "M2513A", 6);

	// Revision (XM6 version number)
	sprintf(rev, "0%01d%01d%01d",
				(int)major, (int)(minor >> 4), (int)(minor & 0x0f));
	memcpy(&buf[32], rev, 4);

	// Size return data
	size = (buf[4] + 5);

	// Limit the size if the buffer is too small
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
BOOL FASTCALL SCSIMO::ModeSelect(const DWORD *cdb, const BYTE *buf, int length)
{
	int page;
	int size;

	ASSERT(this);
	ASSERT(buf);
	ASSERT(length >= 0);

	// PF
	if (cdb[1] & 0x10) {
		// Mode Parameter header
		if (length >= 12) {
			// Check the block length (in bytes)
			size = 1 << disk.size;
			if (buf[9] != (BYTE)(size >> 16) ||
				buf[10] != (BYTE)(size >> 8) || buf[11] != (BYTE)size) {
				// Currently does not allow changing sector length
				disk.code = DISK_INVALIDPRM;
				return FALSE;
			}
			buf += 12;
			length -= 12;
		}

		// Parsing the page
		while (length > 0) {
			// Get the page
			page = buf[0];

			switch (page) {
				// format device
				case 0x03:
					// Check the number of bytes in the physical sector
					size = 1 << disk.size;
					if (buf[0xc] != (BYTE)(size >> 8) ||
						buf[0xd] != (BYTE)size) {
						// Currently does not allow changing sector length
						disk.code = DISK_INVALIDPRM;
						return FALSE;
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
	disk.code = DISK_NOERROR;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Vendor Unique Format Page 20h (MO)
//
//---------------------------------------------------------------------------
int FASTCALL SCSIMO::AddVendor(int page, BOOL change, BYTE *buf)
{
	ASSERT(this);
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

	if (disk.ready) {
		unsigned spare = 0;
		unsigned bands = 0;

		if (disk.size == 9) switch (disk.blocks) {
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

		if (disk.size == 11) switch (disk.blocks) {
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

		buf[2] = 0; // format mode
		buf[3] = 0; // type of format
		buf[4] = (BYTE)(disk.blocks >> 24);
		buf[5] = (BYTE)(disk.blocks >> 16);
		buf[6] = (BYTE)(disk.blocks >> 8);
		buf[7] = (BYTE)disk.blocks;
		buf[8] = (BYTE)(spare >> 8);
		buf[9] = (BYTE)spare;
		buf[10] = (BYTE)(bands >> 8);
		buf[11] = (BYTE)bands;
	}

	return 12;
}

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
//	[ SCSI NEC "Genuine" Hard Disk]
//
//---------------------------------------------------------------------------

#include "scsihd_nec.h"
#include "fileio.h"

//===========================================================================
//
//	SCSI hard disk (PC-9801-55 NEC genuine /Anex86/T98Next)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
SCSIHD_NEC::SCSIHD_NEC() : SCSIHD()
{
	// Work initialization
	cylinders = 0;
	heads = 0;
	sectors = 0;
	sectorsize = 0;
	imgoffset = 0;
	imgsize = 0;
}

//---------------------------------------------------------------------------
//
//	Extract words that are supposed to be little endian
//
//---------------------------------------------------------------------------
static inline WORD getWordLE(const BYTE *b)
{
	return ((WORD)(b[1]) << 8) | b[0];
}

//---------------------------------------------------------------------------
//
//	Extract longwords assumed to be little endian
//
//---------------------------------------------------------------------------
static inline DWORD getDwordLE(const BYTE *b)
{
	return ((DWORD)(b[3]) << 24) | ((DWORD)(b[2]) << 16) |
		((DWORD)(b[1]) << 8) | b[0];
}

//---------------------------------------------------------------------------
//
//	Open
//
//---------------------------------------------------------------------------
BOOL SCSIHD_NEC::Open(const Filepath& path, BOOL /*attn*/)
{
	Fileio fio;
	off64_t size;
	BYTE hdr[512];
	LPCTSTR ext;

	ASSERT(!disk.ready);

	// Open as read-only
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}

	// Get file size
	size = fio.GetFileSize();

	// Read header
	if (size >= (off64_t)sizeof(hdr)) {
		if (!fio.Read(hdr, sizeof(hdr))) {
			fio.Close();
			return FALSE;
		}
	}
	fio.Close();

	// Must be in 512 byte units
	if (size & 0x1ff) {
		return FALSE;
	}

	// 10MB or more
	if (size < 0x9f5400) {
		return FALSE;
	}
	// 2TB according to xm6i
	if (size > 2LL * 1024 * 1024 * 1024 * 1024) {
		return FALSE;
	}

	// Determine parameters by extension
	ext = path.GetFileExt();
	if (xstrcasecmp(ext, _T(".HDN")) == 0) {
		// Assuming sector size 512, number of sectors 25, number of heads 8 as default settings
		imgoffset = 0;
		imgsize = size;
		sectorsize = 512;
		sectors = 25;
		heads = 8;
		cylinders = (int)(size >> 9);
		cylinders >>= 3;
		cylinders /= 25;
	} else if (xstrcasecmp(ext, _T(".HDI")) == 0) { // Anex86 HD image?
		imgoffset = getDwordLE(&hdr[4 + 4]);
		imgsize = getDwordLE(&hdr[4 + 4 + 4]);
		sectorsize = getDwordLE(&hdr[4 + 4 + 4 + 4]);
		sectors = getDwordLE(&hdr[4 + 4 + 4 + 4 + 4]);
		heads = getDwordLE(&hdr[4 + 4 + 4 + 4 + 4 + 4]);
		cylinders = getDwordLE(&hdr[4 + 4 + 4 + 4 + 4 + 4 + 4]);
	} else if (xstrcasecmp(ext, _T(".NHD")) == 0 &&
		memcmp(hdr, "T98HDDIMAGE.R0\0", 15) == 0) { // T98Next HD image?
		imgoffset = getDwordLE(&hdr[0x10 + 0x100]);
		cylinders = getDwordLE(&hdr[0x10 + 0x100 + 4]);
		heads = getWordLE(&hdr[0x10 + 0x100 + 4 + 4]);
		sectors = getWordLE(&hdr[0x10 + 0x100 + 4 + 4 + 2]);
		sectorsize = getWordLE(&hdr[0x10 + 0x100 + 4 + 4 + 2 + 2]);
		imgsize = (off64_t)cylinders * heads * sectors * sectorsize;
	}

	// Supports 256 or 512 sector sizes
	if (sectorsize != 256 && sectorsize != 512) {
		return FALSE;
	}

	// Image size consistency check
	if (imgoffset + imgsize > size || (imgsize % sectorsize != 0)) {
		return FALSE;
	}

	// Sector size
	for(disk.size = 16; disk.size > 0; --(disk.size)) {
		if ((1 << disk.size) == sectorsize)
			break;
	}
	if (disk.size <= 0 || disk.size > 16) {
		return FALSE;
	}

	// Number of blocks
	disk.blocks = (DWORD)(imgsize >> disk.size);
	disk.imgoffset = imgoffset;

	// Call the base class
	return Disk::Open(path);
}

//---------------------------------------------------------------------------
//
//	INQUIRY
//
//---------------------------------------------------------------------------
int SCSIHD_NEC::Inquiry(
	const DWORD *cdb, BYTE *buf, DWORD major, DWORD minor)
{
	int size;

	// Base class
	size = SCSIHD::Inquiry(cdb, buf, major, minor);

	// Exit if there is an error in the base class
	if (size == 0) {
		return 0;
	}

	// Changed to equivalent to SCSI-1
	buf[2] = 0x01;
	buf[3] = 0x01;

	// Replace Vendor name
	buf[8] = 'N';
	buf[9] = 'E';
	buf[10] = 'C';

	return size;
}

//---------------------------------------------------------------------------
//
//	Error page added
//
//---------------------------------------------------------------------------
int SCSIHD_NEC::AddError(BOOL change, BYTE *buf)
{
	ASSERT(buf);

	// Set the message length
	buf[0] = 0x01;
	buf[1] = 0x06;

	// No changeable area
	if (change) {
		return 8;
	}

	// The retry count is 0, and the limit time uses the default value inside the device.
	return 8;
}

//---------------------------------------------------------------------------
//
//	Format page added
//
//---------------------------------------------------------------------------
int SCSIHD_NEC::AddFormat(BOOL change, BYTE *buf)
{
	ASSERT(buf);

	// Set the message length
	buf[0] = 0x80 | 0x03;
	buf[1] = 0x16;

	// Make the number of bytes in the physical sector appear mutable (although it cannot actually be)
	if (change) {
		buf[0xc] = 0xff;
		buf[0xd] = 0xff;
		return 24;
	}

	if (disk.ready) {
		// Set the number of tracks in one zone (PC-9801-55 seems to see this value)
		buf[0x2] = (BYTE)(heads >> 8);
		buf[0x3] = (BYTE)heads;

		// Set the number of sectors per track
		buf[0xa] = (BYTE)(sectors >> 8);
		buf[0xb] = (BYTE)sectors;

		// Set the number of bytes in the physical sector
		int size = 1 << disk.size;
		buf[0xc] = (BYTE)(size >> 8);
		buf[0xd] = (BYTE)size;
	}

	// Set removable attributes (remains of the old days)
	if (disk.removable) {
		buf[20] = 0x20;
	}

	return 24;
}

//---------------------------------------------------------------------------
//
//	Drive page added
//
//---------------------------------------------------------------------------
int SCSIHD_NEC::AddDrive(BOOL change, BYTE *buf)
{
	ASSERT(buf);

	// Set the message length
	buf[0] = 0x04;
	buf[1] = 0x12;

	// No changeable area
	if (change) {
		return 20;
	}

	if (disk.ready) {
		// Set the number of cylinders
		buf[0x2] = (BYTE)(cylinders >> 16);
		buf[0x3] = (BYTE)(cylinders >> 8);
		buf[0x4] = (BYTE)cylinders;

		// Set the number of heads
		buf[0x5] = (BYTE)heads;
	}

	return 20;
}

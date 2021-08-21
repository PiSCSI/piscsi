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
#include "exceptions.h"

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
	SetVendor("NEC");

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
void SCSIHD_NEC::Open(const Filepath& path, BOOL /*attn*/)
{
	ASSERT(!IsReady());

	// Open as read-only
	Fileio fio;
	if (!fio.Open(path, Fileio::ReadOnly)) {
		throw io_exception("Can't open hard disk file read-only");
	}

	// Get file size
	off_t size = fio.GetFileSize();

	// Read header
	BYTE hdr[512];
	if (size >= (off_t)sizeof(hdr)) {
		if (!fio.Read(hdr, sizeof(hdr))) {
			fio.Close();
			throw io_exception("Can't read NEC hard disk file header");
		}
	}
	fio.Close();

	// Must be in 512 byte units
	if (size & 0x1ff) {
		throw io_exception("File size must be a multiple of 512 bytes");
	}

	// 2TB is the current maximum
	if (size > 2LL * 1024 * 1024 * 1024 * 1024) {
		throw io_exception("File size must not exceed 2 TB");
	}

	// Determine parameters by extension
	LPCTSTR ext = path.GetFileExt();
	if (strcasecmp(ext, _T(".HDN")) == 0) {
		// Assuming sector size 512, number of sectors 25, number of heads 8 as default settings
		imgoffset = 0;
		imgsize = size;
		sectorsize = 512;
		sectors = 25;
		heads = 8;
		cylinders = (int)(size >> 9);
		cylinders >>= 3;
		cylinders /= 25;
	} else if (strcasecmp(ext, _T(".HDI")) == 0) { // Anex86 HD image?
		imgoffset = getDwordLE(&hdr[4 + 4]);
		imgsize = getDwordLE(&hdr[4 + 4 + 4]);
		sectorsize = getDwordLE(&hdr[4 + 4 + 4 + 4]);
		sectors = getDwordLE(&hdr[4 + 4 + 4 + 4 + 4]);
		heads = getDwordLE(&hdr[4 + 4 + 4 + 4 + 4 + 4]);
		cylinders = getDwordLE(&hdr[4 + 4 + 4 + 4 + 4 + 4 + 4]);
	} else if (strcasecmp(ext, _T(".NHD")) == 0 &&
		memcmp(hdr, "T98HDDIMAGE.R0\0", 15) == 0) { // T98Next HD image?
		imgoffset = getDwordLE(&hdr[0x10 + 0x100]);
		cylinders = getDwordLE(&hdr[0x10 + 0x100 + 4]);
		heads = getWordLE(&hdr[0x10 + 0x100 + 4 + 4]);
		sectors = getWordLE(&hdr[0x10 + 0x100 + 4 + 4 + 2]);
		sectorsize = getWordLE(&hdr[0x10 + 0x100 + 4 + 4 + 2 + 2]);
		imgsize = (off_t)cylinders * heads * sectors * sectorsize;
	}

	// Supports 256 or 512 sector sizes
	if (sectorsize != 256 && sectorsize != 512) {
		throw io_exception("Sector size must be 256 or 512 bytes");
	}

	// Image size consistency check
	if (imgoffset + imgsize > size || (imgsize % sectorsize != 0)) {
		throw io_exception("Image size consistency check failed");
	}

	// Sector size
	// TODO Do not use disk.size directly
	for(disk.size = 16; disk.size > 0; --(disk.size)) {
		if ((1 << disk.size) == sectorsize)
			break;
	}
	if (disk.size <= 0 || disk.size > 16) {
		throw io_exception("Invalid disk size");
	}

	// Number of blocks
	SetBlockCount((DWORD)(imgsize >> disk.size));
	disk.imgoffset = imgoffset;

	LOGINFO("Media capacity for image file '%s': %d blocks", path.GetPath(), GetBlockCount());

	Disk::Open(path);
	FileSupport::SetPath(path);
}

//---------------------------------------------------------------------------
//
//	INQUIRY
//
//---------------------------------------------------------------------------
int SCSIHD_NEC::Inquiry(const DWORD *cdb, BYTE *buf)
{
	int size = SCSIHD::Inquiry(cdb, buf);

	// This drive is a SCSI-1 SCCS drive
	buf[2] = 0x01;
	buf[3] = 0x01;

	return size;
}

//---------------------------------------------------------------------------
//
//	Error page added
//
//---------------------------------------------------------------------------
int SCSIHD_NEC::AddError(bool change, BYTE *buf)
{
	ASSERT(buf);

	// Set the message length
	buf[0] = 0x01;
	buf[1] = 0x06;

	// The retry count is 0, and the limit time uses the default value inside the device.
	return 8;
}

//---------------------------------------------------------------------------
//
//	Format page added
//
//---------------------------------------------------------------------------
int SCSIHD_NEC::AddFormat(bool change, BYTE *buf)
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

	if (IsReady()) {
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
	if (IsRemovable()) {
		buf[20] = 0x20;
	}

	return 24;
}

//---------------------------------------------------------------------------
//
//	Drive page added
//
//---------------------------------------------------------------------------
int SCSIHD_NEC::AddDrive(bool change, BYTE *buf)
{
	ASSERT(buf);

	// Set the message length
	buf[0] = 0x04;
	buf[1] = 0x12;

	// No changeable area
	if (!change && IsReady()) {
		// Set the number of cylinders
		buf[0x2] = (BYTE)(cylinders >> 16);
		buf[0x3] = (BYTE)(cylinders >> 8);
		buf[0x4] = (BYTE)cylinders;

		// Set the number of heads
		buf[0x5] = (BYTE)heads;
	}

	return 20;
}

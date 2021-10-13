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

SCSIHD_NEC::SCSIHD_NEC() : SCSIHD(false)
{
	// Work initialization
	cylinders = 0;
	heads = 0;
	sectors = 0;
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
	return ((DWORD)(b[3]) << 24) | ((DWORD)(b[2]) << 16) | ((DWORD)(b[1]) << 8) | b[0];
}

void SCSIHD_NEC::Open(const Filepath& path)
{
	ASSERT(!IsReady());

	// Open as read-only
	Fileio fio;
	if (!fio.Open(path, Fileio::ReadOnly)) {
		throw file_not_found_exception("Can't open hard disk file");
	}

	// Get file size
	off_t size = fio.GetFileSize();

	// NEC root sector
	BYTE root_sector[512];
	if (size >= (off_t)sizeof(root_sector)) {
		if (!fio.Read(root_sector, sizeof(root_sector))) {
			fio.Close();
			throw io_exception("Can't read NEC hard disk file root sector");
		}
	}
	fio.Close();

	// Effective size must be a multiple of 512
	size = (size / 512) * 512;

	int image_size = 0;
	int sector_size = 0;

	// Determine parameters by extension
	const char *ext = path.GetFileExt();

	// PC-9801-55 NEC genuine?
	if (!strcasecmp(ext, ".hdn")) {
		// Assuming sector size 512, number of sectors 25, number of heads 8 as default settings
		disk.image_offset = 0;
		image_size = size;
		sector_size = 512;
		sectors = 25;
		heads = 8;
		cylinders = (int)(size >> 9);
		cylinders >>= 3;
		cylinders /= 25;
	}
	// Anex86 HD image?
	else if (!strcasecmp(ext, ".hdi")) {
		disk.image_offset = getDwordLE(&root_sector[8]);
		image_size = getDwordLE(&root_sector[12]);
		sector_size = getDwordLE(&root_sector[16]);
		sectors = getDwordLE(&root_sector[20]);
		heads = getDwordLE(&root_sector[24]);
		cylinders = getDwordLE(&root_sector[28]);
	}
	// T98Next HD image?
	else if (!strcasecmp(ext, ".nhd")) {
		if (!memcmp(root_sector, "T98HDDIMAGE.R0\0", 15)) {
			disk.image_offset = getDwordLE(&root_sector[0x110]);
			cylinders = getDwordLE(&root_sector[0x114]);
			heads = getWordLE(&root_sector[0x118]);
			sectors = getWordLE(&root_sector[0x11a]);
			sector_size = getWordLE(&root_sector[0x11c]);
			image_size = (off_t)cylinders * heads * sectors * sector_size;
		}
		else {
			throw io_exception("Invalid NEC image file format");
		}
	}

	// Image size consistency check
	if (disk.image_offset + image_size > size || (image_size % sector_size != 0)) {
		throw io_exception("Image size consistency check failed");
	}

	// Calculate sector size
	for (size = 16; size > 0; --size) {
		if ((1 << size) == sector_size)
			break;
	}
	if (size <= 0 || size > 16) {
		throw io_exception("Invalid NEC disk size");
	}
	SetSectorSizeShiftCount(size);

	// Number of blocks
	SetBlockCount(image_size >> disk.size);

	FinalizeSetup(path, size);
}

int SCSIHD_NEC::Inquiry(const DWORD *cdb, BYTE *buf)
{
	int size = SCSIHD::Inquiry(cdb, buf);

	// This drive is a SCSI-1 SCCS drive
	buf[2] = 0x01;
	buf[3] = 0x01;

	return size;
}

int SCSIHD_NEC::AddErrorPage(bool change, BYTE *buf)
{
	ASSERT(buf);

	// Set the message length
	buf[0] = 0x01;
	buf[1] = 0x06;

	// The retry count is 0, and the limit time uses the default value inside the device.
	return 8;
}

int SCSIHD_NEC::AddFormatPage(bool change, BYTE *buf)
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

int SCSIHD_NEC::AddDrivePage(bool change, BYTE *buf)
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

//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//  	Copyright (C) akuker
//
//  	Licensed under the BSD 3-Clause License. 
//  	See LICENSE file in the project root folder.
//
//  	[ SASI hard disk ]
//
//---------------------------------------------------------------------------
#include "sasihd.h"
#include "xm6.h"
#include "fileio.h"
#include "exceptions.h"

//===========================================================================
//
//	SASI Hard Disk
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
SASIHD::SASIHD() : Disk("SAHD")
{
	SetProtectable(true);
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void SASIHD::Reset()
{
	// Unlock, clear attention
	SetLocked(false);
	SetAttn(false);

	// Reset, clear the code
	SetReset(false);
	SetStatusCode(STATUS_NOERROR);
}

//---------------------------------------------------------------------------
//
//	Open
//
//---------------------------------------------------------------------------
void SASIHD::Open(const Filepath& path)
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

	#if defined(USE_MZ1F23_1024_SUPPORT)
	// For MZ-2500 / MZ-2800 MZ-1F23 (SASI 20M / sector size 1024) only
	// 20M(22437888 BS=1024 C=21912)
	if (size == 0x1566000) {
		// Sector size and number of blocks
		SetSectorSize(10);
		SetBlockCount((DWORD)(size >> 10));

		Disk::Open(path);
		FileSupport::SetPath(path);
	}
	#endif	// USE_MZ1F23_1024_SUPPORT

	#if defined(REMOVE_FIXED_SASIHD_SIZE)
	// Must be in 256-byte units
	if (size & 0xff) {
		throw io_exception("File size must be a multiple of 256 bytes");
	}

	// 10MB or more
	if (size < 0x9f5400) {
		throw io_exception("File size must be at least 10 MB");
	}

	// Limit to about 512MB
	if (size > 512 * 1024 * 1024) {
		throw io_exception("File size must not exceed 512 MB");
	}
	#else
	// 10MB, 20MB, 40MBのみ
	switch (size) {
		// 10MB (10441728 BS=256 C=40788)
		case 0x9f5400:
			break;

		// 20MB (20748288 BS=256 C=81048)
		case 0x13c9800:
			break;

		// 40MB (41496576 BS=256 C=162096)
		case 0x2793000:
			break;

		// Other (Not supported )
		default:
			throw io_exception("Unsupported file size");
	}
	#endif	// REMOVE_FIXED_SASIHD_SIZE

	// Sector size 256 bytes and number of blocks
	SetSectorSize(8);
	SetBlockCount((DWORD)(size >> 8));

	// Call the base class
	Disk::Open(path);
}

//---------------------------------------------------------------------------
//
//	REQUEST SENSE
//
//---------------------------------------------------------------------------
int SASIHD::RequestSense(const DWORD *cdb, BYTE *buf)
{
	ASSERT(cdb);
	ASSERT(buf);

	// Size decision
	int size = (int)cdb[4];
	ASSERT(size >= 0 && size < 0x100);

	// Transfer 4 bytes when size 0 (Shugart Associates System Interface specification)
	if (size == 0) {
		size = 4;
	}

	// SASI fixed to non-extended format
	memset(buf, 0, size);
	buf[0] = (BYTE)(GetStatusCode() >> 16);
	buf[1] = (BYTE)(GetLun() << 5);

	return size;
}

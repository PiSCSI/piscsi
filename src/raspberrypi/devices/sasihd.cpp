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
#include "fileio.h"
#include "exceptions.h"
#include "../config.h"

SASIHD::SASIHD(const set<uint32_t>& sector_sizes) : Disk("SAHD")
{
	SetSectorSizes(sector_sizes);
}

void SASIHD::Reset()
{
	// Unlock, clear attention
	SetLocked(false);
	SetAttn(false);

	// Reset, clear the code
	SetReset(false);
	SetStatusCode(STATUS_NOERROR);
}

void SASIHD::Open(const Filepath& path)
{
	assert(!IsReady());

	// Open as read-only
	Fileio fio;
	if (!fio.Open(path, Fileio::ReadOnly)) {
		throw file_not_found_exception("Can't open SASI hard disk file");
	}

	// Get file size
	off_t size = fio.GetFileSize();
	fio.Close();

	// Sector size (default 256 bytes) and number of blocks
	SetSectorSizeInBytes(GetConfiguredSectorSize() ? GetConfiguredSectorSize() : 256, true);
	SetBlockCount((DWORD)(size >> GetSectorSizeShiftCount()));

	#if defined(REMOVE_FIXED_SASIHD_SIZE)
	// Effective size must be a multiple of the sector size
	size = (size / GetSectorSizeInBytes()) * GetSectorSizeInBytes();
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

	Disk::Open(path);
	FileSupport::SetPath(path);
}

vector<BYTE> SASIHD::Inquiry(const DWORD *) const
{
	return vector<BYTE>(0);
}

int SASIHD::RequestSense(const DWORD *cdb, BYTE *buf)
{
	// Size decision
	int size = (int)cdb[4];
	assert(size >= 0 && size < 0x100);

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

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
//  [ SASI hard disk ]
//
//---------------------------------------------------------------------------
#include "sasihd.h"
#include "xm6.h"
#include "fileio.h"


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
SASIHD::SASIHD() : Disk()
{
	// SASI ハードディスク
	disk.id = MAKEID('S', 'A', 'H', 'D');
}

//---------------------------------------------------------------------------
//
//	リセット
//
//---------------------------------------------------------------------------
void FASTCALL SASIHD::Reset()
{
	// ロック状態解除、アテンション解除
	disk.lock = FALSE;
	disk.attn = FALSE;

	// Resetなし、コードをクリア
	disk.reset = FALSE;
	disk.code = 0x00;
}

//---------------------------------------------------------------------------
//
//	オープン
//
//---------------------------------------------------------------------------
BOOL FASTCALL SASIHD::Open(const Filepath& path, BOOL /*attn*/)
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

#if defined(USE_MZ1F23_1024_SUPPORT)
	// MZ-2500/MZ-2800用 MZ-1F23(SASI 20M/セクタサイズ1024)専用
	// 20M(22437888 BS=1024 C=21912)
	if (size == 0x1566000) {
		// セクタサイズとブロック数
		disk.size = 10;
		disk.blocks = (DWORD)(size >> 10);

		// Call the base class
		return Disk::Open(path);
	}
#endif	// USE_MZ1F23_1024_SUPPORT

#if defined(REMOVE_FIXED_SASIHD_SIZE)
	// 256バイト単位であること
	if (size & 0xff) {
		return FALSE;
	}

	// 10MB以上
	if (size < 0x9f5400) {
		return FALSE;
	}

	// 512MB程度に制限しておく
	if (size > 512 * 1024 * 1024) {
		return FALSE;
	}
#else
	// 10MB, 20MB, 40MBのみ
	switch (size) {
		// 10MB(10441728 BS=256 C=40788)
		case 0x9f5400:
			break;

		// 20MB(20748288 BS=256 C=81048)
		case 0x13c9800:
			break;

		// 40MB(41496576 BS=256 C=162096)
		case 0x2793000:
			break;

		// Other(サポートしない)
		default:
			return FALSE;
	}
#endif	// REMOVE_FIXED_SASIHD_SIZE

	// セクタサイズとブロック数
	disk.size = 8;
	disk.blocks = (DWORD)(size >> 8);

	// Call the base class
	return Disk::Open(path);
}

//---------------------------------------------------------------------------
//
//	REQUEST SENSE
//
//---------------------------------------------------------------------------
int FASTCALL SASIHD::RequestSense(const DWORD *cdb, BYTE *buf)
{
	int size;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(buf);

	// サイズ決定
	size = (int)cdb[4];
	ASSERT((size >= 0) && (size < 0x100));

	// サイズ0のときに4バイト転送する(Shugart Associates System Interface仕様)
	if (size == 0) {
		size = 4;
	}

	// SASIは非拡張フォーマットに固定
	memset(buf, 0, size);
	buf[0] = (BYTE)(disk.code >> 16);
	buf[1] = (BYTE)(disk.lun << 5);

	// コードをクリア
	disk.code = 0x00;

	return size;
}

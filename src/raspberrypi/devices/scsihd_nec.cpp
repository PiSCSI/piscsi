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
//  [ SCSI NEC "Genuine" Hard Disk]
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
	// ワーク初期化
	cylinders = 0;
	heads = 0;
	sectors = 0;
	sectorsize = 0;
	imgoffset = 0;
	imgsize = 0;
}

//---------------------------------------------------------------------------
//
//	リトルエンディアンと想定したワードを取り出す
//
//---------------------------------------------------------------------------
static inline WORD getWordLE(const BYTE *b)
{
	return ((WORD)(b[1]) << 8) | b[0];
}

//---------------------------------------------------------------------------
//
//	リトルエンディアンと想定したロングワードを取り出す
//
//---------------------------------------------------------------------------
static inline DWORD getDwordLE(const BYTE *b)
{
	return ((DWORD)(b[3]) << 24) | ((DWORD)(b[2]) << 16) |
		((DWORD)(b[1]) << 8) | b[0];
}

//---------------------------------------------------------------------------
//
//	オープン
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSIHD_NEC::Open(const Filepath& path, BOOL /*attn*/)
{
	Fileio fio;
	off64_t size;
	BYTE hdr[512];
	LPCTSTR ext;

	ASSERT(this);
	ASSERT(!disk.ready);

	// Open as read-only
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}

	// Get file size
	size = fio.GetFileSize();

	// ヘッダー読み込み
	if (size >= (off64_t)sizeof(hdr)) {
		if (!fio.Read(hdr, sizeof(hdr))) {
			fio.Close();
			return FALSE;
		}
	}
	fio.Close();

	// 512バイト単位であること
	if (size & 0x1ff) {
		return FALSE;
	}

	// 10MB以上
	if (size < 0x9f5400) {
		return FALSE;
	}
	// xm6iに準じて2TB
	// よく似たものが wxw/wxw_cfg.cpp にもある
	if (size > 2LL * 1024 * 1024 * 1024 * 1024) {
		return FALSE;
	}

	// 拡張子別にパラメータを決定
	ext = path.GetFileExt();
	if (xstrcasecmp(ext, _T(".HDN")) == 0) {
		// デフォルト設定としてセクタサイズ512,セクタ数25,ヘッド数8を想定
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

	// セクタサイズは256または512をサポート
	if (sectorsize != 256 && sectorsize != 512) {
		return FALSE;
	}

	// イメージサイズの整合性チェック
	if (imgoffset + imgsize > size || (imgsize % sectorsize != 0)) {
		return FALSE;
	}

	// セクタサイズ
	for(disk.size = 16; disk.size > 0; --(disk.size)) {
		if ((1 << disk.size) == sectorsize)
			break;
	}
	if (disk.size <= 0 || disk.size > 16) {
		return FALSE;
	}

	// ブロック数
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
int FASTCALL SCSIHD_NEC::Inquiry(
	const DWORD *cdb, BYTE *buf, DWORD major, DWORD minor)
{
	int size;

	// 基底クラス
	size = SCSIHD::Inquiry(cdb, buf, major, minor);

	// 基底クラスでエラーなら終了
	if (size == 0) {
		return 0;
	}

	// SCSI1相当に変更
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
//	エラーページ追加
//
//---------------------------------------------------------------------------
int FASTCALL SCSIHD_NEC::AddError(BOOL change, BYTE *buf)
{
	ASSERT(this);
	ASSERT(buf);

	// Set the message length
	buf[0] = 0x01;
	buf[1] = 0x06;

	// No changeable area
	if (change) {
		return 8;
	}

	// リトライカウントは0、リミットタイムは装置内部のデフォルト値を使用
	return 8;
}

//---------------------------------------------------------------------------
//
//	フォーマットページ追加
//
//---------------------------------------------------------------------------
int FASTCALL SCSIHD_NEC::AddFormat(BOOL change, BYTE *buf)
{
	int size;

	ASSERT(this);
	ASSERT(buf);

	// Set the message length
	buf[0] = 0x80 | 0x03;
	buf[1] = 0x16;

	// 物理セクタのバイト数は変更可能に見せる(実際には変更できないが)
	if (change) {
		buf[0xc] = 0xff;
		buf[0xd] = 0xff;
		return 24;
	}

	if (disk.ready) {
		// 1ゾーンのトラック数を設定(PC-9801-55はこの値を見ているようだ)
		buf[0x2] = (BYTE)(heads >> 8);
		buf[0x3] = (BYTE)heads;

		// 1トラックのセクタ数を設定
		buf[0xa] = (BYTE)(sectors >> 8);
		buf[0xb] = (BYTE)sectors;

		// 物理セクタのバイト数を設定
		size = 1 << disk.size;
		buf[0xc] = (BYTE)(size >> 8);
		buf[0xd] = (BYTE)size;
	}

	// リムーバブル属性を設定(昔の名残)
	if (disk.removable) {
		buf[20] = 0x20;
	}

	return 24;
}

//---------------------------------------------------------------------------
//
//	ドライブページ追加
//
//---------------------------------------------------------------------------
int FASTCALL SCSIHD_NEC::AddDrive(BOOL change, BYTE *buf)
{
	ASSERT(this);
	ASSERT(buf);

	// Set the message length
	buf[0] = 0x04;
	buf[1] = 0x12;

	// No changeable area
	if (change) {
		return 20;
	}

	if (disk.ready) {
		// シリンダ数を設定
		buf[0x2] = (BYTE)(cylinders >> 16);
		buf[0x3] = (BYTE)(cylinders >> 8);
		buf[0x4] = (BYTE)cylinders;

		// ヘッド数を設定
		buf[0x5] = (BYTE)heads;
	}

	return 20;
}

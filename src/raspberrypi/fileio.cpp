//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2010-2018 GIMONS
//	[ ファイルI/O(RaSCSI用サブセット) ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "filepath.h"
#include "fileio.h"

//===========================================================================
//
//	ファイルI/O
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	コンストラクタ
//
//---------------------------------------------------------------------------
Fileio::Fileio()
{
	// ワーク初期化
	handle = -1;
}

//---------------------------------------------------------------------------
//
//	デストラクタ
//
//---------------------------------------------------------------------------
Fileio::~Fileio()
{
	ASSERT(handle == -1);

	// Releaseでの安全策
	Close();
}

//---------------------------------------------------------------------------
//
//	ロード
//
//---------------------------------------------------------------------------
BOOL FASTCALL Fileio::Load(const Filepath& path, void *buffer, int size)
{
	ASSERT(this);
	ASSERT(buffer);
	ASSERT(size > 0);
	ASSERT(handle < 0);

	// オープン
	if (!Open(path, ReadOnly)) {
		return FALSE;
	}

	// 読み込み
	if (!Read(buffer, size)) {
		Close();
		return FALSE;
	}

	// クローズ
	Close();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	セーブ
//
//---------------------------------------------------------------------------
BOOL FASTCALL Fileio::Save(const Filepath& path, void *buffer, int size)
{
	ASSERT(this);
	ASSERT(buffer);
	ASSERT(size > 0);
	ASSERT(handle < 0);

	// オープン
	if (!Open(path, WriteOnly)) {
		return FALSE;
	}

	// 読み込み
	if (!Write(buffer, size)) {
		Close();
		return FALSE;
	}

	// クローズ
	Close();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	オープン
//
//---------------------------------------------------------------------------
BOOL FASTCALL Fileio::Open(LPCTSTR fname, OpenMode mode)
{
	ASSERT(this);
	ASSERT(fname);
	ASSERT(handle < 0);

	// ヌル文字列からの読み込みは必ず失敗させる
	if (fname[0] == _T('\0')) {
		handle = -1;
		return FALSE;
	}

	// モード別
	switch (mode) {
		// 読み込みのみ
		case ReadOnly:
			handle = open(fname, O_RDONLY);
			break;

		// 書き込みのみ
		case WriteOnly:
			handle = open(fname, O_CREAT | O_WRONLY | O_TRUNC, 0666);
			break;

		// 読み書き両方
		case ReadWrite:
			// CD-ROMからの読み込みはRWが成功してしまう
			if (access(fname, 0x06) != 0) {
				return FALSE;
			}
			handle = open(fname, O_RDWR);
			break;

		// アペンド
		case Append:
			handle = open(fname, O_CREAT | O_WRONLY | O_APPEND, 0666);
			break;

		// それ以外
		default:
			ASSERT(FALSE);
			break;
	}

	// 結果評価
	if (handle == -1) {
		return FALSE;
	}
	ASSERT(handle >= 0);
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	オープン
//
//---------------------------------------------------------------------------
BOOL FASTCALL Fileio::OpenDIO(LPCTSTR fname, OpenMode mode)
{
	ASSERT(this);
	ASSERT(fname);
	ASSERT(handle < 0);

	// ヌル文字列からの読み込みは必ず失敗させる
	if (fname[0] == _T('\0')) {
		handle = -1;
		return FALSE;
	}

	// モード別
	switch (mode) {
		// 読み込みのみ
		case ReadOnly:
			handle = open(fname, O_RDONLY | O_DIRECT);
			break;

		// 書き込みのみ
		case WriteOnly:
			handle = open(fname, O_CREAT | O_WRONLY | O_TRUNC | O_DIRECT, 0666);
			break;

		// 読み書き両方
		case ReadWrite:
			// CD-ROMからの読み込みはRWが成功してしまう
			if (access(fname, 0x06) != 0) {
				return FALSE;
			}
			handle = open(fname, O_RDWR | O_DIRECT);
			break;

		// アペンド
		case Append:
			handle = open(fname, O_CREAT | O_WRONLY | O_APPEND | O_DIRECT, 0666);
			break;

		// それ以外
		default:
			ASSERT(FALSE);
			break;
	}

	// 結果評価
	if (handle == -1) {
		return FALSE;
	}
	ASSERT(handle >= 0);
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	オープン
//
//---------------------------------------------------------------------------
BOOL FASTCALL Fileio::Open(const Filepath& path, OpenMode mode)
{
	ASSERT(this);

	return Open(path.GetPath(), mode);
}

//---------------------------------------------------------------------------
//
//	オープン
//
//---------------------------------------------------------------------------
BOOL FASTCALL Fileio::OpenDIO(const Filepath& path, OpenMode mode)
{
	ASSERT(this);

	return OpenDIO(path.GetPath(), mode);
}

//---------------------------------------------------------------------------
//
//	読み込み
//
//---------------------------------------------------------------------------
BOOL FASTCALL Fileio::Read(void *buffer, int size)
{
	int count;

	ASSERT(this);
	ASSERT(buffer);
	ASSERT(size > 0);
	ASSERT(handle >= 0);

	// 読み込み
	count = read(handle, buffer, size);
	if (count != size) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	書き込み
//
//---------------------------------------------------------------------------
BOOL FASTCALL Fileio::Write(const void *buffer, int size)
{
	int count;

	ASSERT(this);
	ASSERT(buffer);
	ASSERT(size > 0);
	ASSERT(handle >= 0);

	// 読み込み
	count = write(handle, buffer, size);
	if (count != size) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	シーク
//
//---------------------------------------------------------------------------
BOOL FASTCALL Fileio::Seek(off64_t offset, BOOL relative)
{
	ASSERT(this);
	ASSERT(handle >= 0);
	ASSERT(offset >= 0);

	// 相対シークならオフセットに現在値を追加
	if (relative) {
		offset += GetFilePos();
	}

	if (lseek(handle, offset, SEEK_SET) != offset) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ファイルサイズ取得
//
//---------------------------------------------------------------------------
off64_t FASTCALL Fileio::GetFileSize()
{
	off64_t cur;
	off64_t end;

	ASSERT(this);
	ASSERT(handle >= 0);

	// ファイル位置を64bitで取得
	cur = GetFilePos();

	// ファイルサイズを64bitで取得
	end = lseek(handle, 0, SEEK_END);

	// 位置を元に戻す
	Seek(cur);

	return end;
}

//---------------------------------------------------------------------------
//
//	ファイル位置取得
//
//---------------------------------------------------------------------------
off64_t FASTCALL Fileio::GetFilePos() const
{
	off64_t pos;

	ASSERT(this);
	ASSERT(handle >= 0);

	// ファイル位置を64bitで取得
	pos = lseek(handle, 0, SEEK_CUR);
	return pos;

}

//---------------------------------------------------------------------------
//
//	クローズ
//
//---------------------------------------------------------------------------
void FASTCALL Fileio::Close()
{
	ASSERT(this);

	if (handle != -1) {
		close(handle);
		handle = -1;
	}
}

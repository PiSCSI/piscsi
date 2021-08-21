//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2010-2020 GIMONS
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
BOOL Fileio::Load(const Filepath& path, void *buffer, int size)
{
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
BOOL Fileio::Save(const Filepath& path, void *buffer, int size)
{
	ASSERT(buffer);
	ASSERT(size > 0);
	ASSERT(handle < 0);

	// オープン
	if (!Open(path, WriteOnly)) {
		return FALSE;
	}

	// 書き込み
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
BOOL Fileio::Open(LPCTSTR fname, OpenMode mode, BOOL directIO)
{
	mode_t omode;

	ASSERT(fname);
	ASSERT(handle < 0);

	// ヌル文字列からの読み込みは必ず失敗させる
	if (fname[0] == _T('\0')) {
		handle = -1;
		return FALSE;
	}

	// デフォルトモード
	omode = directIO ? O_DIRECT : 0;

	// モード別
	switch (mode) {
		// 読み込みのみ
		case ReadOnly:
			handle = open(fname, O_RDONLY | omode);
			break;

		// 書き込みのみ
		case WriteOnly:
			handle = open(fname, O_CREAT | O_WRONLY | O_TRUNC | omode, 0666);
			break;

		// 読み書き両方
		case ReadWrite:
			// CD-ROMからの読み込みはRWが成功してしまう
			if (access(fname, 0x06) != 0) {
				return FALSE;
			}
			handle = open(fname, O_RDWR | omode);
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
BOOL Fileio::Open(LPCTSTR fname, OpenMode mode)
{

	return Open(fname, mode, FALSE);
}

//---------------------------------------------------------------------------
//
//	オープン
//
//---------------------------------------------------------------------------
BOOL Fileio::Open(const Filepath& path, OpenMode mode)
{

	return Open(path.GetPath(), mode);
}

//---------------------------------------------------------------------------
//
//	オープン
//
//---------------------------------------------------------------------------
BOOL Fileio::OpenDIO(LPCTSTR fname, OpenMode mode)
{

	// O_DIRECT付きでオープン
	if (!Open(fname, mode, TRUE)) {
		// 通常モードリトライ(tmpfs等)
		return Open(fname, mode, FALSE);
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	オープン
//
//---------------------------------------------------------------------------
BOOL Fileio::OpenDIO(const Filepath& path, OpenMode mode)
{

	return OpenDIO(path.GetPath(), mode);
}

//---------------------------------------------------------------------------
//
//	読み込み
//
//---------------------------------------------------------------------------
BOOL Fileio::Read(void *buffer, int size)
{
	int count;

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
BOOL Fileio::Write(const void *buffer, int size)
{
	int count;

	ASSERT(buffer);
	ASSERT(size > 0);
	ASSERT(handle >= 0);

	// 書き込み
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
BOOL Fileio::Seek(off_t offset, BOOL relative)
{
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
off_t Fileio::GetFileSize()
{
	off_t cur;
	off_t end;

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
off_t Fileio::GetFilePos() const
{
	off_t pos;

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
void Fileio::Close()
{

	if (handle != -1) {
		close(handle);
		handle = -1;
	}
}

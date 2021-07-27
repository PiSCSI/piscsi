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
bool Fileio::Load(const Filepath& path, void *buffer, int size)
{
	ASSERT(buffer);
	ASSERT(size > 0);
	ASSERT(handle < 0);

	// オープン
	if (!Open(path, ReadOnly)) {
		return false;
	}

	// 読み込み
	if (!Read(buffer, size)) {
		Close();
		return false;
	}

	// クローズ
	Close();

	return true;
}

//---------------------------------------------------------------------------
//
//	セーブ
//
//---------------------------------------------------------------------------
bool Fileio::Save(const Filepath& path, void *buffer, int size)
{
	ASSERT(buffer);
	ASSERT(size > 0);
	ASSERT(handle < 0);

	// オープン
	if (!Open(path, WriteOnly)) {
		return false;
	}

	// 書き込み
	if (!Write(buffer, size)) {
		Close();
		return false;
	}

	// クローズ
	Close();

	return true;
}

//---------------------------------------------------------------------------
//
//	オープン
//
//---------------------------------------------------------------------------
bool Fileio::Open(LPCTSTR fname, OpenMode mode, bool directIO)
{
	mode_t omode;

	ASSERT(fname);
	ASSERT(handle < 0);

	// ヌル文字列からの読み込みは必ず失敗させる
	if (fname[0] == _T('\0')) {
		handle = -1;
		return false;
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
				return false;
			}
			handle = open(fname, O_RDWR | omode);
			break;

		// アペンド
		case Append:
			handle = open(fname, O_CREAT | O_WRONLY | O_APPEND | omode, 0666);
			break;

		// それ以外
		default:
			ASSERT(false);
			break;
	}

	// 結果評価
	if (handle == -1) {
		return false;
	}

	ASSERT(handle >= 0);
	return true;
}

//---------------------------------------------------------------------------
//
//	オープン
//
//---------------------------------------------------------------------------
bool Fileio::Open(LPCTSTR fname, OpenMode mode)
{

	return Open(fname, mode, false);
}

//---------------------------------------------------------------------------
//
//	オープン
//
//---------------------------------------------------------------------------
bool Fileio::Open(const Filepath& path, OpenMode mode)
{

	return Open(path.GetPath(), mode);
}

//---------------------------------------------------------------------------
//
//	オープン
//
//---------------------------------------------------------------------------
bool Fileio::OpenDIO(LPCTSTR fname, OpenMode mode)
{

	// O_DIRECT付きでオープン
	if (!Open(fname, mode, true)) {
		// 通常モードリトライ(tmpfs等)
		return Open(fname, mode, false);
	}

	return true;
}

//---------------------------------------------------------------------------
//
//	オープン
//
//---------------------------------------------------------------------------
bool Fileio::OpenDIO(const Filepath& path, OpenMode mode)
{

	return OpenDIO(path.GetPath(), mode);
}

//---------------------------------------------------------------------------
//
//	読み込み
//
//---------------------------------------------------------------------------
bool Fileio::Read(void *buffer, int size)
{
	int count;

	ASSERT(buffer);
	ASSERT(size > 0);
	ASSERT(handle >= 0);

	// 読み込み
	count = read(handle, buffer, size);
	if (count != size) {
		return false;
	}

	return true;
}

//---------------------------------------------------------------------------
//
//	書き込み
//
//---------------------------------------------------------------------------
bool Fileio::Write(const void *buffer, int size)
{
	int count;

	ASSERT(buffer);
	ASSERT(size > 0);
	ASSERT(handle >= 0);

	// 書き込み
	count = write(handle, buffer, size);
	if (count != size) {
		return false;
	}

	return true;
}

//---------------------------------------------------------------------------
//
//	シーク
//
//---------------------------------------------------------------------------
bool Fileio::Seek(off64_t offset, bool relative)
{
	ASSERT(handle >= 0);
	ASSERT(offset >= 0);

	// 相対シークならオフセットに現在値を追加
	if (relative) {
		offset += GetFilePos();
	}

	if (lseek(handle, offset, SEEK_SET) != offset) {
		return false;
	}

	return true;
}

//---------------------------------------------------------------------------
//
//	ファイルサイズ取得
//
//---------------------------------------------------------------------------
off64_t Fileio::GetFileSize()
{
	off64_t cur;
	off64_t end;

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
off64_t Fileio::GetFilePos() const
{
	off64_t pos;

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

//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2013-2020 GIMONS
//	[ ファイルI/O(RaSCSI用サブセット) ]
//
//---------------------------------------------------------------------------

#if !defined(fileio_h)
#define fileio_h

#include "filepath.h"

//===========================================================================
//
//	マクロ(Load,Save用)
//
//===========================================================================
#define PROP_IMPORT(f, p) \
	if (!f->Read(&(p), sizeof(p))) {\
		return false;\
	}\

#define PROP_EXPORT(f, p) \
	if (!f->Write(&(p), sizeof(p))) {\
		return false;\
	}\

//===========================================================================
//
//	ファイルI/O
//
//===========================================================================
class Fileio
{
public:
	enum OpenMode {
		ReadOnly,						// 読み込みのみ
		WriteOnly,						// 書き込みのみ
		ReadWrite,						// 読み書き両方
		Append							// アペンド
	};

public:
	Fileio();
										// コンストラクタ
	virtual ~Fileio();
										// デストラクタ
	bool Load(const Filepath& path, void *buffer, int size);
										// ROM,RAMロード
	bool Save(const Filepath& path, void *buffer, int size);
										// RAMセーブ

	bool Open(LPCTSTR fname, OpenMode mode);
										// オープン
	bool Open(const Filepath& path, OpenMode mode);
										// オープン
	bool OpenDIO(LPCTSTR fname, OpenMode mode);
										// オープン
	bool OpenDIO(const Filepath& path, OpenMode mode);
										// オープン
	bool Seek(off64_t offset, bool relative = false);
										// シーク
	bool Read(void *buffer, int size);
										// 読み込み
	bool Write(const void *buffer, int size);
										// 書き込み
	off64_t GetFileSize();
										// ファイルサイズ取得
	off64_t GetFilePos() const;
										// ファイル位置取得
	void Close();
										// クローズ
	bool IsValid() const		{ return (bool)(handle != -1); }
										// 有効チェック

private:
	bool Open(LPCTSTR fname, OpenMode mode, bool directIO);
										// オープン

	int handle;							// ファイルハンドル
};

#endif	// fileio_h

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
		return FALSE;\
	}\

#define PROP_EXPORT(f, p) \
	if (!f->Write(&(p), sizeof(p))) {\
		return FALSE;\
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
		ReadWrite						// 読み書き両方
	};

public:
	Fileio();
										// コンストラクタ
	virtual ~Fileio();
										// デストラクタ
	BOOL Load(const Filepath& path, void *buffer, int size);
										// ROM,RAMロード
	BOOL Save(const Filepath& path, void *buffer, int size);
										// RAMセーブ

	BOOL Open(LPCTSTR fname, OpenMode mode);
										// オープン
	BOOL Open(const Filepath& path, OpenMode mode);
										// オープン
	BOOL OpenDIO(LPCTSTR fname, OpenMode mode);
										// オープン
	BOOL OpenDIO(const Filepath& path, OpenMode mode);
										// オープン
	BOOL Seek(off_t offset, BOOL relative = FALSE);
										// シーク
	BOOL Read(void *buffer, int size);
										// 読み込み
	BOOL Write(const void *buffer, int size);
										// 書き込み
	off_t GetFileSize();
										// ファイルサイズ取得
	off_t GetFilePos() const;
										// ファイル位置取得
	void Close();
										// クローズ
	BOOL IsValid() const		{ return (BOOL)(handle != -1); }
										// 有効チェック

private:
	BOOL Open(LPCTSTR fname, OpenMode mode, BOOL directIO);
										// オープン

	int handle;							// ファイルハンドル
};

#endif	// fileio_h

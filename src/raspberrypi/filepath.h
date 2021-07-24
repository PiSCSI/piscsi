//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2012-2020 GIMONS
//	[ ファイルパス(サブセット) ]
//
//---------------------------------------------------------------------------

#if !defined(filepath_h)
#define filepath_h

class Fileio;

//---------------------------------------------------------------------------
//
//	Constant definition
//
//---------------------------------------------------------------------------
#define FILEPATH_MAX		_MAX_PATH

//===========================================================================
//
//	ファイルパス
//	※代入演算子を用意すること
//
//===========================================================================
class Filepath
{
public:
	Filepath();
										// コンストラクタ
	virtual ~Filepath();
										// デストラクタ
	Filepath& operator=(const Filepath& path);
										// 代入

	void Clear();
										// クリア
	void SetPath(LPCSTR path);
										// ファイル設定(ユーザ) MBCS用
	LPCTSTR GetPath() const	{ return m_szPath; }
										// パス名取得
	LPCTSTR GetFileExt() const;
										// ショート名取得(LPCTSTR)
	BOOL Save(Fileio *fio, int ver);
										// セーブ
	BOOL Load(Fileio *fio, int ver);
										// ロード

private:
	void Split();
										// パス分割
	TCHAR m_szPath[_MAX_PATH];
										// ファイルパス
	TCHAR m_szDir[_MAX_DIR];
										// ディレクトリ
	TCHAR m_szFile[_MAX_FNAME];
										// ファイル
	TCHAR m_szExt[_MAX_EXT];
										// 拡張子

	static TCHAR FileExt[_MAX_FNAME + _MAX_DIR];
										// ショート名(TCHAR)
};

#endif	// filepath_h

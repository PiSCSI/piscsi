//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2012-2018 GIMONS
//	[ ファイルパス(サブセット) ]
//
//---------------------------------------------------------------------------

#if !defined(filepath_h)
#define filepath_h

//---------------------------------------------------------------------------
//
//	定数定義
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

	void FASTCALL Clear();
										// クリア
	void FASTCALL SetPath(LPCSTR path);
										// ファイル設定(ユーザ) MBCS用
	BOOL FASTCALL IsClear() const;
										// クリアされているか
	LPCTSTR FASTCALL GetPath() const	{ return m_szPath; }
										// パス名取得
	const char* FASTCALL GetShort() const;
										// ショート名取得(const char*)
	LPCTSTR FASTCALL GetFileExt() const;
										// ショート名取得(LPCTSTR)
	BOOL FASTCALL CmpPath(const Filepath& path) const;
										// パス比較

	BOOL FASTCALL Save(Fileio *fio, int ver);
										// セーブ
	BOOL FASTCALL Load(Fileio *fio, int ver);
										// ロード

private:
	void FASTCALL Split();
										// パス分割
	void FASTCALL Make();
										// パス合成
	void FASTCALL SetCurDir();
										// カレントディレクトリ設定
	TCHAR m_szPath[_MAX_PATH];
										// ファイルパス
	TCHAR m_szDrive[_MAX_DRIVE];
										// ドライブ
	TCHAR m_szDir[_MAX_DIR];
										// ディレクトリ
	TCHAR m_szFile[_MAX_FNAME];
										// ファイル
	TCHAR m_szExt[_MAX_EXT];
										// 拡張子

	static char ShortName[_MAX_FNAME + _MAX_DIR];
										// ショート名(char)
	static TCHAR FileExt[_MAX_FNAME + _MAX_DIR];
										// ショート名(TCHAR)
};

#endif	// filepath_h

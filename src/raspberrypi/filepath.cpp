//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2012-2020 GIMONS
//	[ ファイルパス(サブセット) ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "filepath.h"
#include "fileio.h"

//===========================================================================
//
//	ファイルパス
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	コンストラクタ
//
//---------------------------------------------------------------------------
Filepath::Filepath()
{
	// クリア
	Clear();
}

//---------------------------------------------------------------------------
//
//	デストラクタ
//
//---------------------------------------------------------------------------
Filepath::~Filepath()
{
}

//---------------------------------------------------------------------------
//
//	代入演算子
//
//---------------------------------------------------------------------------
Filepath& Filepath::operator=(const Filepath& path)
{
	// パス設定(内部でSplitされる)
	SetPath(path.GetPath());

	return *this;
}

//---------------------------------------------------------------------------
//
//	クリア
//
//---------------------------------------------------------------------------
void FASTCALL Filepath::Clear()
{
	ASSERT(this);

	// パスおよび各部分をクリア
	m_szPath[0] = _T('\0');
	m_szDir[0] = _T('\0');
	m_szFile[0] = _T('\0');
	m_szExt[0] = _T('\0');
}

//---------------------------------------------------------------------------
//
//	ファイル設定(ユーザ) MBCS用
//
//---------------------------------------------------------------------------
void FASTCALL Filepath::SetPath(LPCSTR path)
{
	ASSERT(this);
	ASSERT(path);
	ASSERT(strlen(path) < _MAX_PATH);

	// パス名コピー
	strcpy(m_szPath, (LPTSTR)path);

	// 分離
	Split();
}

#ifdef BAREMETAL
//---------------------------------------------------------------------------
//
//	互換関数(dirname) 結果は直ぐにコピーせよ
//
//---------------------------------------------------------------------------
static char dirtmp[2];
char* dirname(char *path)
{
	char *p;
	if( path == NULL || *path == '\0' ) {
		dirtmp[0] = '.';
		dirtmp[1] = '\0';
		return dirtmp;
	}

	p = path + strlen(path) - 1;
	while( *p == '/' ) {
		if( p == path )
			return path;
		*p-- = '\0';
	}

	while( p >= path && *p != '/' ) {
		p--;
	}

	if (p < path) {
		dirtmp[0] = '.';
		dirtmp[1] = '\0';
		return dirtmp;
	}

	if (p == path) {
		dirtmp[0] = '/';
		dirtmp[1] = '\0';
		return dirtmp;
	}

	*p = 0;
	return path;
}

//---------------------------------------------------------------------------
//
//	互換関数(basename) 結果は直ぐにコピーせよ
//
//---------------------------------------------------------------------------
static char basetmp[2];
char* basename(char *path)
{
	char *p;
	if( path == NULL || *path == '\0' ) {
		basetmp[0] = '/';
		basetmp[1] = '\0';
		return basetmp;
	}

	p = path + strlen(path) - 1;
	while( *p == '/' ) {
		if( p == path ) {
			return path;
		}
		*p-- = '\0';
	}

	while( p >= path && *p != '/' ) {
		p--;
	}

	return p + 1;
}
#endif	// BAREMETAL

//---------------------------------------------------------------------------
//
//	パス分離
//
//---------------------------------------------------------------------------
void FASTCALL Filepath::Split()
{
	LPTSTR pDir;
	LPTSTR pDirName;
	LPTSTR pBase;
	LPTSTR pBaseName;
	LPTSTR pExtName;

	ASSERT(this);

	// パーツを初期化
	m_szDir[0] = _T('\0');
	m_szFile[0] = _T('\0');
	m_szExt[0] = _T('\0');

	// 分離
	pDir = strdup(m_szPath);
	pDirName = dirname(pDir);
	pBase = strdup(m_szPath);
	pBaseName = basename(pBase);
	pExtName = strrchr(pBaseName, '.');

	// 転送
	if (pDirName) {
		strcpy(m_szDir, pDirName);
		strcat(m_szDir, "/");
	}

	if (pExtName) {
		strcpy(m_szExt, pExtName);
	}

	if (pBaseName) {
		strcpy(m_szFile, pBaseName);
	}

	// 解放
	free(pDir);
	free(pBase);
}

//---------------------------------------------------------------------------
//
//	パス合成
//
//---------------------------------------------------------------------------
void FASTCALL Filepath::Make()
{
	ASSERT(this);

	// パス初期化
	m_szPath[0] = _T('\0');

	// 合成
	strncat(m_szPath, m_szDir, ARRAY_SIZE(m_szPath) - strlen(m_szPath));
	strncat(m_szPath, m_szFile, ARRAY_SIZE(m_szPath) - strlen(m_szPath));
	strncat(m_szPath, m_szExt, ARRAY_SIZE(m_szPath) - strlen(m_szPath));
}

//---------------------------------------------------------------------------
//
//	クリアされているか
//
//---------------------------------------------------------------------------
BOOL FASTCALL Filepath::IsClear() const
{
	// Clear()の逆
	if ((m_szPath[0] == _T('\0')) &&
		(m_szDir[0] == _T('\0')) &&
		(m_szFile[0] == _T('\0')) &&
		(m_szExt[0] == _T('\0'))) {
		// 確かに、クリアされている
		return TRUE;
	}

	// クリアされていない
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	ショート名取得
//	※返されるポインタは一時的なもの。すぐコピーすること
//	※FDIDiskのdisk.nameとの関係で、文字列は最大59文字+終端とすること
//
//---------------------------------------------------------------------------
const char* FASTCALL Filepath::GetShort() const
{
	char szFile[_MAX_FNAME];
	char szExt[_MAX_EXT];

	ASSERT(this);

	// TCHAR文字列からchar文字列へ変換
	strcpy(szFile, m_szFile);
	strcpy(szExt, m_szExt);

	// 固定バッファへ合成
	strcpy(ShortName, szFile);
	strcat(ShortName, szExt);

	// strlenで調べたとき、最大59になるように細工
	ShortName[59] = '\0';

	// const charとして返す
	return (const char*)ShortName;
}

//---------------------------------------------------------------------------
//
//	ファイル名＋拡張子取得
//	※返されるポインタは一時的なもの。すぐコピーすること
//
//---------------------------------------------------------------------------
LPCTSTR FASTCALL Filepath::GetFileExt() const
{
	ASSERT(this);

	// 固定バッファへ合成
	strcpy(FileExt, m_szExt);

	// LPCTSTRとして返す
	return (LPCTSTR)FileExt;
}

//---------------------------------------------------------------------------
//
//	パス比較
//
//---------------------------------------------------------------------------
BOOL FASTCALL Filepath::CmpPath(const Filepath& path) const
{
	// パスが完全一致していればTRUE
	if (strcmp(path.GetPath(), GetPath()) == 0) {
		return TRUE;
	}

	return FALSE;
}

//---------------------------------------------------------------------------
//
//	セーブ
//
//---------------------------------------------------------------------------
BOOL FASTCALL Filepath::Save(Fileio *fio, int /*ver*/)
{
	ASSERT(this);
	ASSERT(fio);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ロード
//
//---------------------------------------------------------------------------
BOOL FASTCALL Filepath::Load(Fileio *fio, int /*ver*/)
{
	ASSERT(this);
	ASSERT(fio);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ショート名
//
//---------------------------------------------------------------------------
char Filepath::ShortName[_MAX_FNAME + _MAX_DIR];

//---------------------------------------------------------------------------
//
//	ファイル名＋拡張子
//
//---------------------------------------------------------------------------
TCHAR Filepath::FileExt[_MAX_FNAME + _MAX_DIR];

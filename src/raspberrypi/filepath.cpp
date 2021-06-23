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
//	File path
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
Filepath::Filepath()
{
	// Clear
	Clear();
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
Filepath::~Filepath()
{
}

//---------------------------------------------------------------------------
//
//	Assignment operator
//
//---------------------------------------------------------------------------
Filepath& Filepath::operator=(const Filepath& path)
{
	// Set path (split internally)
	SetPath(path.GetPath());

	return *this;
}

//---------------------------------------------------------------------------
//
//      Clear
//
//---------------------------------------------------------------------------
void FASTCALL Filepath::Clear()
{
	ASSERT(this);

	// Clear the path and each part
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

	// Copy pathname
	strcpy(m_szPath, (LPTSTR)path);

	// Split
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
//	File name + extension acquisition
//	The returned pointer is temporary. Copy immediately.
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
//	Save
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
//	Load
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
//	Filename and extension
//
//---------------------------------------------------------------------------
TCHAR Filepath::FileExt[_MAX_FNAME + _MAX_DIR];

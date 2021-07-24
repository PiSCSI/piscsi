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
void Filepath::Clear()
{

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
void Filepath::SetPath(LPCSTR path)
{
	ASSERT(path);
	ASSERT(strlen(path) < _MAX_PATH);

	// Copy pathname
	strcpy(m_szPath, (LPTSTR)path);

	// Split
	Split();
}

//---------------------------------------------------------------------------
//
//	パス分離
//
//---------------------------------------------------------------------------
void Filepath::Split()
{
	LPTSTR pDir;
	LPTSTR pDirName;
	LPTSTR pBase;
	LPTSTR pBaseName;
	LPTSTR pExtName;


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
LPCTSTR Filepath::GetFileExt() const
{

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
BOOL Filepath::Save(Fileio *fio, int /*ver*/)
{
	ASSERT(fio);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL Filepath::Load(Fileio *fio, int /*ver*/)
{
	ASSERT(fio);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Filename and extension
//
//---------------------------------------------------------------------------
TCHAR Filepath::FileExt[_MAX_FNAME + _MAX_DIR];

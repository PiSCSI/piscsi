//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2012-2020 GIMONS
//	[ File path (subset) ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "filepath.h"
#include "fileio.h"
#include "rascsi.h"

//===========================================================================
//
//	File path
//
//===========================================================================

Filepath::Filepath()
{
	// Clear
	Clear();
}

Filepath::~Filepath()
{
}

Filepath& Filepath::operator=(const Filepath& path)
{
	// Set path (split internally)
	SetPath(path.GetPath());

	return *this;
}

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
//	File settings (user) for MBCS
//
//---------------------------------------------------------------------------
void Filepath::SetPath(const char *path)
{
	ASSERT(path);
	ASSERT(strlen(path) < _MAX_PATH);

	// Copy pathname
	strcpy(m_szPath, (char *)path);

	// Split
	Split();
}

//---------------------------------------------------------------------------
//
//	Split paths
//
//---------------------------------------------------------------------------
void Filepath::Split()
{
	// Initialize the parts
	m_szDir[0] = _T('\0');
	m_szFile[0] = _T('\0');
	m_szExt[0] = _T('\0');

	// Split
	char *pDir = strdup(m_szPath);
	char *pDirName = dirname(pDir);
	char *pBase = strdup(m_szPath);
	char *pBaseName = basename(pBase);
	char *pExtName = strrchr(pBaseName, '.');

	// Transmit
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

	// Release
	free(pDir);
	free(pBase);
}

//---------------------------------------------------------------------------
//
//	File name + extension acquisition
//	The returned pointer is temporary. Copy immediately.
//
//---------------------------------------------------------------------------
const char *Filepath::GetFileExt() const
{

	// Merge into static buffer
	strcpy(FileExt, m_szExt);

	// Return as LPCTSTR
	return (const char *)FileExt;
}

BOOL Filepath::Save(Fileio *fio, int /*ver*/)
{
	ASSERT(fio);

	return TRUE;
}

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

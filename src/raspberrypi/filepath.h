//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2012-2020 GIMONS
//	[ File path (subset) ]
//
//---------------------------------------------------------------------------

#pragma once

#include "os.h"

class Fileio;

//---------------------------------------------------------------------------
//
//	Constant definition
//
//---------------------------------------------------------------------------
#define FILEPATH_MAX		_MAX_PATH

//===========================================================================
//
//	File path
//	Assignment operators are prepared here
//
//===========================================================================
class Filepath
{
public:
	Filepath();
	virtual ~Filepath();
	Filepath& operator=(const Filepath& path);

	void Clear();
	void SetPath(const char *path);		// File settings (user) for MBCS
	const char *GetPath() const	{ return m_szPath; }	// Get path name
	const char *GetFileExt() const;		// Get short name (LPCTSTR)
	BOOL Save(Fileio *fio, int ver);
	BOOL Load(Fileio *fio, int ver);

private:
	void Split();						// Split the path
	TCHAR m_szPath[_MAX_PATH];			// File path
	TCHAR m_szDir[_MAX_DIR];			// Directory
	TCHAR m_szFile[_MAX_FNAME];			// File
	TCHAR m_szExt[_MAX_EXT];			// Extension

	static TCHAR FileExt[_MAX_FNAME + _MAX_DIR];	// Short name (TCHAR)
};

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

using TCHAR = char;

static const int _MAX_EXT = 256;
static const int _MAX_DIR = 256;
static const int _MAX_PATH = 260;
static const int _MAX_FNAME = 256;
static const int FILEPATH_MAX = _MAX_PATH;

//===========================================================================
//
//	File path
//	Assignment operators are prepared here
//
//===========================================================================
class Filepath
{
public:

	Filepath() = default;
	~Filepath() = default;
	Filepath(Filepath&) = default;
	Filepath& operator=(const Filepath&);

	void SetPath(const char *);		// File settings (user) for MBCS
	const char *GetPath() const	{ return m_szPath; }	// Get path name
	const char *GetFileExt() const;		// Get short name (LPCTSTR)

private:

	void Split();						// Split the path
	TCHAR m_szPath[_MAX_PATH] = {};		// File path
	TCHAR m_szDir[_MAX_DIR] = {};		// Directory
	TCHAR m_szFile[_MAX_FNAME] = {};	// File
	TCHAR m_szExt[_MAX_EXT] = {};		// Extension

	static TCHAR FileExt[_MAX_FNAME + _MAX_DIR];	// Short name (TCHAR)
};

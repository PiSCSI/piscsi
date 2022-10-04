//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2013-2020 GIMONS
//	[ File I/O (Subset for RaSCSI) ]
//
//---------------------------------------------------------------------------

#pragma once

#include "filepath.h"

class Fileio
{
public:

	enum class OpenMode {
		ReadOnly,
		WriteOnly,
		ReadWrite
	};

	Fileio() = default;
	virtual ~Fileio();
	Fileio(Fileio&) = default;
	Fileio& operator=(const Fileio&) = default;

	bool Open(const char *fname, OpenMode mode);
	bool Open(const Filepath& path, OpenMode mode);
	bool OpenDIO(const Filepath& path, OpenMode mode);
	bool Seek(off_t offset) const;
	bool Read(BYTE *buffer, int size) const;
	bool Write(const BYTE *buffer, int size) const;
	off_t GetFileSize() const;
	void Close();

private:

	bool Open(const char *fname, OpenMode mode, bool directIO);
	bool OpenDIO(const char *fname, OpenMode mode);

	int handle = -1;
};

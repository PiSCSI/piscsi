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

#include <cstdint>
#include <cstdlib>

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
	bool Read(uint8_t *buffer, int size) const;
	bool Write(const uint8_t *buffer, int size) const;
	off_t GetFileSize() const;
	void Close();

private:

	bool Open(const char *fname, OpenMode mode, bool directIO);

	int handle = -1;
};

//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2010-2020 GIMONS
//	[ File I/O (Subset for RaSCSI) ]
//
//---------------------------------------------------------------------------

#include "fileio.h"
#include <fcntl.h>
#include <unistd.h>
#include <cassert>

Fileio::~Fileio()
{
	// Safety measure for Release
	Close();
}

bool Fileio::Open(const char *fname, OpenMode mode, bool directIO)
{
	assert(fname);
	assert(handle < 0);

	// Always fail a read from a null array
	if (fname[0] == '\0') {
		handle = -1;
		return false;
	}

	// Default mode
	mode_t omode = directIO ? O_DIRECT : 0;

	switch (mode) {
		case OpenMode::ReadOnly:
			handle = open(fname, O_RDONLY | omode);
			break;

		case OpenMode::WriteOnly:
			handle = open(fname, O_CREAT | O_WRONLY | O_TRUNC | omode, 0666);
			break;

		case OpenMode::ReadWrite:
			handle = open(fname, O_RDWR | omode);
			break;

		default:
			assert(false);
			break;
	}

	// Evaluate results
	return handle != -1;
}

bool Fileio::Open(const char *fname, OpenMode mode)
{
	return Open(fname, mode, false);
}

bool Fileio::Open(const Filepath& path, OpenMode mode)
{
	return Open(path.GetPath(), mode);
}

bool Fileio::OpenDIO(const char *fname, OpenMode mode)
{
	// Open with included O_DIRECT
	if (!Open(fname, mode, true)) {
		// Normal mode retry (tmpfs etc.)
		return Open(fname, mode, false);
	}

	return true;
}

bool Fileio::OpenDIO(const Filepath& path, OpenMode mode)
{
	return OpenDIO(path.GetPath(), mode);
}

bool Fileio::Read(BYTE *buffer, int size) const
{
	assert(buffer);
	assert(size > 0);
	assert(handle >= 0);

	return read(handle, buffer, size) == size;
}

bool Fileio::Write(const BYTE *buffer, int size) const
{
	assert(buffer);
	assert(size > 0);
	assert(handle >= 0);

	return write(handle, buffer, size) == size;
}

bool Fileio::Seek(off_t offset) const
{
	assert(handle >= 0);
	assert(offset >= 0);

	return lseek(handle, offset, SEEK_SET) == offset;
}

off_t Fileio::GetFileSize() const
{
	assert(handle >= 0);

	// Get file position in 64bit
	off_t cur = lseek(handle, 0, SEEK_CUR);

	// Get file size in64bitで
	off_t end = lseek(handle, 0, SEEK_END);

	// Return to start position
	Seek(cur);

	return end;
}

void Fileio::Close()
{
	if (handle != -1) {
		close(handle);
		handle = -1;
	}
}

//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2010-2020 GIMONS
//	[ File I/O (Subset for RaSCSI) ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "filepath.h"
#include "fileio.h"
#include "rascsi.h"

//===========================================================================
//
//	File I/O
//
//===========================================================================

Fileio::Fileio()
{
	// Initialize work
	handle = -1;
}

Fileio::~Fileio()
{
	ASSERT(handle == -1);

	// Safety measure for Release
	Close();
}

BOOL Fileio::Load(const Filepath& path, void *buffer, int size)
{
	ASSERT(buffer);
	ASSERT(size > 0);
	ASSERT(handle < 0);

	if (!Open(path, ReadOnly)) {
		return FALSE;
	}

	if (!Read(buffer, size)) {
		Close();
		return FALSE;
	}

	Close();

	return TRUE;
}

BOOL Fileio::Save(const Filepath& path, void *buffer, int size)
{
	ASSERT(buffer);
	ASSERT(size > 0);
	ASSERT(handle < 0);

	if (!Open(path, WriteOnly)) {
		return FALSE;
	}

	if (!Write(buffer, size)) {
		Close();
		return FALSE;
	}

	Close();

	return TRUE;
}

BOOL Fileio::Open(const char *fname, OpenMode mode, BOOL directIO)
{
	mode_t omode;

	ASSERT(fname);
	ASSERT(handle < 0);

	// Always fail a read from a null array
	if (fname[0] == _T('\0')) {
		handle = -1;
		return FALSE;
	}

	// Default mode
	omode = directIO ? O_DIRECT : 0;

	switch (mode) {
		case ReadOnly:
			handle = open(fname, O_RDONLY | omode);
			break;

		case WriteOnly:
			handle = open(fname, O_CREAT | O_WRONLY | O_TRUNC | omode, 0666);
			break;

		case ReadWrite:
			// Make sure RW does not succeed when reading from CD-ROM
			if (access(fname, 0x06) != 0) {
				return FALSE;
			}
			handle = open(fname, O_RDWR | omode);
			break;

		default:
			ASSERT(FALSE);
			break;
	}

	// Evaluate results
	if (handle == -1) {
		return FALSE;
	}

	ASSERT(handle >= 0);
	return TRUE;
}

BOOL Fileio::Open(const char *fname, OpenMode mode)
{

	return Open(fname, mode, FALSE);
}

BOOL Fileio::Open(const Filepath& path, OpenMode mode)
{

	return Open(path.GetPath(), mode);
}

BOOL Fileio::OpenDIO(const char *fname, OpenMode mode)
{

	// Open with included O_DIRECT
	if (!Open(fname, mode, TRUE)) {
		// Normal mode retry (tmpfs etc.)
		return Open(fname, mode, FALSE);
	}

	return TRUE;
}

BOOL Fileio::OpenDIO(const Filepath& path, OpenMode mode)
{

	return OpenDIO(path.GetPath(), mode);
}

BOOL Fileio::Read(void *buffer, int size)
{
	int count;

	ASSERT(buffer);
	ASSERT(size > 0);
	ASSERT(handle >= 0);

	count = read(handle, buffer, size);
	if (count != size) {
		return FALSE;
	}

	return TRUE;
}

BOOL Fileio::Write(const void *buffer, int size)
{
	int count;

	ASSERT(buffer);
	ASSERT(size > 0);
	ASSERT(handle >= 0);

	count = write(handle, buffer, size);
	if (count != size) {
		return FALSE;
	}

	return TRUE;
}

BOOL Fileio::Seek(off_t offset, BOOL relative)
{
	ASSERT(handle >= 0);
	ASSERT(offset >= 0);

	// Add current position in case of relative seek
	if (relative) {
		offset += GetFilePos();
	}

	if (lseek(handle, offset, SEEK_SET) != offset) {
		return FALSE;
	}

	return TRUE;
}

off_t Fileio::GetFileSize()
{
	off_t cur;
	off_t end;

	ASSERT(handle >= 0);

	// Get file position in 64bit
	cur = GetFilePos();

	// Get file size in64bitで
	end = lseek(handle, 0, SEEK_END);

	// Return to start position
	Seek(cur);

	return end;
}

off_t Fileio::GetFilePos() const
{
	off_t pos;

	ASSERT(handle >= 0);

	// Get file position in 64bit
	pos = lseek(handle, 0, SEEK_CUR);
	return pos;

}

void Fileio::Close()
{

	if (handle != -1) {
		close(handle);
		handle = -1;
	}
}

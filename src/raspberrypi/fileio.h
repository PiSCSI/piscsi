//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2013-2020 GIMONS
//	[ File I/O (Subset for RaSCSI) ]
//
//---------------------------------------------------------------------------

#if !defined(fileio_h)
#define fileio_h

#include "filepath.h"

//===========================================================================
//
//	Macros (for Load, Save)
//
//===========================================================================
#define PROP_IMPORT(f, p) \
	if (!f->Read(&(p), sizeof(p))) {\
		return FALSE;\
	}\

#define PROP_EXPORT(f, p) \
	if (!f->Write(&(p), sizeof(p))) {\
		return FALSE;\
	}\

//===========================================================================
//
//	File I/O
//
//===========================================================================
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
	Fileio(Fileio&) = delete;
	Fileio& operator=(const Fileio&) = delete;

	BOOL Open(const char *fname, OpenMode mode);
	BOOL Open(const Filepath& path, OpenMode mode);
	BOOL OpenDIO(const char *fname, OpenMode mode);
	BOOL OpenDIO(const Filepath& path, OpenMode mode);
	BOOL Seek(off_t offset, BOOL relative = FALSE) const;
	BOOL Read(BYTE *buffer, int size) const;
	BOOL Write(const BYTE *buffer, int size) const;
	off_t GetFileSize() const;
	off_t GetFilePos() const;
	void Close();

private:
	BOOL Open(const char *fname, OpenMode mode, BOOL directIO);

	int handle = -1;						// File handle
};

#endif	// fileio_h

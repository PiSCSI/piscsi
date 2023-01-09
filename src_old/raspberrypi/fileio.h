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
	enum OpenMode {
		ReadOnly,
		WriteOnly,
		ReadWrite
	};

public:
	Fileio();
	virtual ~Fileio();
	BOOL Load(const Filepath& path, void *buffer, int size);	// Load ROM, RAM
	BOOL Save(const Filepath& path, void *buffer, int size);	// Save RAM

	BOOL Open(const char *fname, OpenMode mode);
	BOOL Open(const Filepath& path, OpenMode mode);
	BOOL OpenDIO(const char *fname, OpenMode mode);
	BOOL OpenDIO(const Filepath& path, OpenMode mode);
	BOOL Seek(off_t offset, BOOL relative = FALSE);
	BOOL Read(void *buffer, int size);
	BOOL Write(const void *buffer, int size);
	off_t GetFileSize();
	off_t GetFilePos() const;
	void Close();
	BOOL IsValid() const		{ return (BOOL)(handle != -1); }

private:
	BOOL Open(const char *fname, OpenMode mode, BOOL directIO);

	int handle;							// File handle
};

#endif	// fileio_h
